/**
 * @file abstractservice.h  Abstract SIP Service interface definition
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2014  Metaswitch Networks Ltd
 *
 * Parts of this module were derived from GPL licensed PJSIP sample code
 * with the following copyrights.
 *   Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 *   Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#ifndef ABSTRACTSERVICE_H__
#define ABSTRACTSERVICE_H__

extern "C" {
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <stdint.h>
}

#include "sas.h"

class ServiceTsx;
class AppServer;
class AppServerTsx;


/// The ServiceTsxHelper class handles the underlying service-related processing of
/// a single transaction.  Once a service has been triggered as part of handling
/// a transaction, the related ServiceTsxHelper is inspected to determine what should
/// be done next, e.g. forward the request, reject it, fork it etc.
/// 
/// This is an abstract base class to allow for alternative implementations -
/// in particular, production and test.  It is implemented by the underlying
/// service infrastructure, not by the services themseves.
///
class ServiceTsxHelper
{
public:
  /// Virtual destructor.
  virtual ~ServiceTsxHelper() {}

  /// Adds the service to the underlying SIP dialog with the specified dialog
  /// identifier.
  ///
  /// @param  dialog_id    - The dialog identifier to be used for this service.
  ///                        If omitted, a default unique identifier is created
  ///                        using parameters from the SIP request.
  ///
  virtual void add_to_dialog(const std::string& dialog_id="") = 0;

  /// Returns the dialog identifier for this service.
  ///
  /// @returns             - The dialog identifier attached to this service,
  ///                        either by this ServiceTsx instance
  ///                        or by an earlier transaction in the same dialog.
  virtual const std::string& dialog_id() const = 0;

  /// Clones the request.  This is typically used when forking a request if
  /// different request modifications are required on each fork.
  ///
  /// @returns             - The cloned request message.
  /// @param  req          - The request message to clone.
  virtual pjsip_msg* clone_request(pjsip_msg* req) = 0;

  /// Adds the specified target as a new target for the request.  If no request
  /// is specified, the originally received request is used.  Each target is
  /// assigned a unique fork identifier, which is passed in with any subsequent
  /// received responses.
  ///
  /// This is logically equivalent to changing the ReqURI of the message and
  /// adding Route: headers for each of the URIs in the `paths` parameter.
  ///
  /// This function may also be used to insert an extra Route without changing
  /// the ReqURI, indicated by not setting the `uri` parameter in the `target`.
  ///
  /// @returns             - The identity of this fork.
  /// @param  request_uri  - The URI for the new target.
  /// @param  req          - The request message to use for this fork.  If NULL
  ///                        the original request message is used.
  virtual int add_target(Target& target,
                         pjsip_msg* req=NULL) = 0;

  /// Add the specified URI as a new target for the request and indicate that
  /// the target should pass the request back to you if it wishes the request
  /// to continue.  If no request is specified, the originally received request
  /// is used.  The supplied token will be provided back on the next call to
  /// `rx_initial_request()` if the target has done this.
  ///
  /// As with `add_target()` each fork is assigned a unique identifier which
  /// will be passed in with any subsequent received responses.
  ///
  /// This is logically equivalent to replacing the unused Route: headers
  /// with a pair of Route headers, first to the request_uri and the second
  /// back to the local service (with `token` added as an ODI token).
  ///
  /// @return              - The identity of this fork.
  /// @param  request_uri  - The URI for the new target.
  /// @param  token        - An opaque token used to corellate returned requests.
  /// @param  req          - The request message to use for this fork.  If NULL
  ///                        the original request message is used.
  virtual int add_returning_target(pjsip_uri* request_uri,
                                   const std::string& token,
                                   pjsip_msg* req=NULL) = 0;

  /// Rejects the original request with the specified status code and text.
  /// This method can only be called when handling the original request.
  /// Any subsequent rejection of the request must be done by sending a final
  /// response using the send_response method.
  ///
  /// @param  status_code  - The SIP status code to send on the response.
  /// @param  status_text  - The SIP status text to send on the response.  If 
  ///                        omitted, the default status text for the code is
  ///                        used (if this is a standard SIP status code).
  virtual void reject(int status_code,
                      const std::string& status_text="") = 0;

  /// Sends a provisional or final response to the transaction.  If a final
  /// response is sent on an INVITE transaction that was forked, all forks 
  /// which have not yet responded are cancelled.
  ///
  /// @param  rsp          - The response message to send.
  virtual void send_response(pjsip_msg* rsp) = 0;

  /// Frees the specified message.  Received responses or messages that have
  /// been cloned with add_target are owned by the AppServerTsx.  It must
  /// call into ServiceTsx either to send them on or to free them (via this
  /// API).
  ///
  /// @param  msg          - The message to free.
  virtual void free_msg(pjsip_msg* msg) = 0;

  /// Returns the pool corresponding to a message.  This pool can then be used
  /// to allocate further headers or bodies to add to the message.
  ///
  /// @returns             - The pool corresponding to this message.
  /// @param  msg          - The message.
  virtual pj_pool_t* get_pool(const pjsip_msg* msg) = 0;

  /// Returns the SAS trail identifier that should be used for any SAS events
  /// related to this service invocation.
  virtual SAS::TrailId trail() const = 0;

};


/// The AppServerTsxHelper class handles the underlying service-related
/// processing of a single transaction for an AppServer.  Once a service has
/// been triggered as part of handling a transaction, the related
/// AppServerTsxHelper is inspected to determine what should be done next, e.g.
/// forward the request, reject it, fork it etc.
/// 
/// This is an abstract base class to allow for alternative implementations -
/// in particular, production and test.  It is implemented by the underlying
/// service infrastructure, not by the services themseves.
///
class AppServerTsxHelper
{
public:
  /// Virtual destructor.
  virtual ~ServiceTsxHelper() {}

  /// Adds the service to the underlying SIP dialog with the specified dialog
  /// identifier.
  ///
  /// @param  dialog_id    - The dialog identifier to be used for this service.
  ///                        If omitted, a default unique identifier is created
  ///                        using parameters from the SIP request.
  ///
  virtual void add_to_dialog(const std::string& dialog_id="") = 0;

  /// Returns the dialog identifier for this service.
  ///
  /// @returns             - The dialog identifier attached to this service,
  ///                        either by this ServiceTsx instance
  ///                        or by an earlier transaction in the same dialog.
  virtual const std::string& dialog_id() const = 0;

  /// Clones the request.  This is typically used when forking a request if
  /// different request modifications are required on each fork.
  ///
  /// @returns             - The cloned request message.
  /// @param  req          - The request message to clone.
  virtual pjsip_msg* clone_request(pjsip_msg* req) = 0;

  /// Adds the specified target as a new target for the request.  If no request
  /// is specified, the originally received request is used.  Each target is
  /// assigned a unique fork identifier, which is passed in with any subsequent
  /// received responses.
  ///
  /// This is logically equivalent to changing the ReqURI of the message and
  /// adding Route: headers for each of the URIs in the `paths` parameter.
  ///
  /// This function may also be used to insert an extra Route without changing
  /// the ReqURI, indicated by not setting the `uri` parameter in the `target`.
  ///
  /// @returns             - The identity of this fork.
  /// @param  request_uri  - The URI for the new target.
  /// @param  req          - The request message to use for this fork.  If NULL
  ///                        the original request message is used.
  virtual int add_target(Target& target,
                         pjsip_msg* req=NULL) = 0; // AMC - Here we shim through to `ServiceTsxHelper::add_target()`, building an appropriate target.

  /// Add the specified URI as a new target for the request and indicate that
  /// the target should pass the request back to you if it wishes the request
  /// to continue.  If no request is specified, the originally received request
  /// is used.  The supplied token will be provided back on the next call to
  /// `rx_initial_request()` if the target has done this.
  ///
  /// As with `add_target()` each fork is assigned a unique identifier which
  /// will be passed in with any subsequent received responses.
  ///
  /// This is logically equivalent to replacing the unused Route: headers
  /// with a pair of Route headers, first to the request_uri and the second
  /// back to the local service (with `token` added as an ODI token).
  ///
  /// @return              - The identity of this fork.
  /// @param  request_uri  - The URI for the new target.
  /// @param  token        - An opaque token used to corellate returned requests.
  /// @param  req          - The request message to use for this fork.  If NULL
  ///                        the original request message is used.
  virtual int add_returning_target(pjsip_uri* request_uri,
                                   const std::string& token,
                                   pjsip_msg* req=NULL) = 0;

  /// Rejects the original request with the specified status code and text.
  /// This method can only be called when handling the original request.
  /// Any subsequent rejection of the request must be done by sending a final
  /// response using the send_response method.
  ///
  /// @param  status_code  - The SIP status code to send on the response.
  /// @param  status_text  - The SIP status text to send on the response.  If 
  ///                        omitted, the default status text for the code is
  ///                        used (if this is a standard SIP status code).
  virtual void reject(int status_code,
                      const std::string& status_text="") = 0;

  /// Sends a provisional or final response to the transaction.  If a final
  /// response is sent on an INVITE transaction that was forked, all forks 
  /// which have not yet responded are cancelled.
  ///
  /// @param  rsp          - The response message to send.
  virtual void send_response(pjsip_msg* rsp) = 0;

  /// Frees the specified message.  Received responses or messages that have
  /// been cloned with add_target are owned by the AppServerTsx.  It must
  /// call into ServiceTsx either to send them on or to free them (via this
  /// API).
  ///
  /// @param  msg          - The message to free.
  virtual void free_msg(pjsip_msg* msg) = 0;

  /// Returns the pool corresponding to a message.  This pool can then be used
  /// to allocate further headers or bodies to add to the message.
  ///
  /// @returns             - The pool corresponding to this message.
  /// @param  msg          - The message.
  virtual pj_pool_t* get_pool(const pjsip_msg* msg) = 0;

  /// Returns the SAS trail identifier that should be used for any SAS events
  /// related to this service invocation.
  virtual SAS::TrailId trail() const = 0;

protected:
  /// Constructor
  AppServerTsxHelper(ServiceTsxHelper* helper) : _helper(helper) {};

private:
  /// Transaction helper to delegate to for underlying service-related processing.
  /// This has a more powerful and flexible API than the one offered by
  /// AppServerTsxHelper and should not be directly exposed on the public API.
  ServiceTsxHelper* _helper;
};

/// The ServiceTsx class is an abstract base class used to handle the
/// application-server-specific processing of a single transaction.  It
/// is provided with a ServiceTsxHelper, which it may use to perform the
/// underlying service-related processing.
///
class ServiceTsx
{
public:
  /// Virtual destructor.
  virtual ~ServiceTsx() {}

  /// Called for an initial request (dialog-initiating or out-of-dialog) with
  /// the original received request for the transaction.  Unless the reject
  /// method is called, on return from this method the request will be
  /// forwarded to all targets added using the add_target API, or to the
  /// existing RequestURI if no targets were added.
  ///
  /// @param req           - The received initial request.
  virtual void on_initial_request(pjsip_msg* req) = 0;

  /// Called for an in-dialog request with the original received request for
  /// the transaction.  Unless the reject method is called, on return from
  /// this method the request will be forwarded within the dialog.
  ///
  /// @param req           - The received in-dialog request.
  virtual void on_in_dialog_request(pjsip_msg* req) {}

  /// Called with all responses received on the transaction.  If a transport
  /// error or transaction timeout occurs on a downstream leg, this method is
  /// called with a 408 response.  The return value indicates whether the 
  /// response should be forwarded upstream (after suitable consolidation if
  /// the request was forked).  If the return value is false and new targets
  /// have been added with the add_target API, the original request is forked
  /// to them.
  ///
  /// @returns             - true if the response should be forwarded upstream
  ///                        false if the response should be dropped
  /// @param  rsp          - The received request.
  /// @param  fork_id      - The identity of the downstream fork on which
  ///                        the response was received.
  virtual bool on_response(pjsip_msg* rsp, int fork_id) {return true;}

  /// Called if the original request is cancelled (either by a received
  /// CANCEL request or an error on the inbound transport).  On return from 
  /// this method the transaction (and any remaining downstream legs) will be
  /// cancelled automatically.  No further methods will be called for this
  /// transaction.
  ///
  /// @param  status_code  - Indicates the reason for the cancellation 
  ///                        (487 for a CANCEL, 408 for a transport error
  ///                        or transaction timeout)
  virtual void on_cancel(int status_code) {}

protected:
  /// Constructor.
  ServiceTsx(ServiceTsxHelper* helper) : _helper(helper) {}

  /// AMC - Add wrapper functions here to call through to the
  /// helper (as for AppServerTsx).

private:
  /// Transaction helper to use for underlying service-related processing.
  ServiceTsxHelper* _helper;
}


/// The AbstractService class is a base class on which SIP services can be
/// built.  
///
/// Derived classes are instantiated during system initialization and 
/// register a service name with Sprout.  Sprout calls the create_tsx method
/// on an AbstractService derived class when the ServiceManager determines that
/// the next hop for a request contains a hostname of the form
/// &lt;service_name&gt;.&lt;homedomain&gt;.  This may happen if:
///
/// -  an initial request is received with a top route header/ReqURI indicating
///    this service.
/// -  an initial request has been forwarded by some earlier service instance
///    to this service.
/// -  an in-dialog request is received for a dialog on which the service
///    previously called add_to_dialog.
///
class AbstractService
{
public:
  /// Virtual destructor.
  virtual ~AbstractService() {}

  /// Called when the system determines the service should be invoked for a
  /// received request.  The AbstractService can either return NULL indicating it
  /// does not want to process the request, or create a suitable object
  /// derived from the ServiceTsx class to process the request.
  ///
  /// @param  helper        - The service helper to use to perform
  ///                         the underlying service-related processing.
  /// @param  req           - The received request message.
  virtual ServiceTsx* get_app_tsx(ServiceTsxHelper* helper,
                                  pjsip_msg* req) = 0;

  /// Returns the name of this service.
  const std::string service_name() { return _service_name; }

protected:
  /// Constructor.
  AbstractService(const std::string& service_name) :
    _service_name(service_name) {}

private:
  /// The name of this service.
  const std::string _service_name;
};


/// The AppServer class is an abstract base class used to implement services.
///
/// Derived classes are instantiated during system initialization and 
/// register a service name with Sprout.  Sprout calls the get_app_tsx method
/// on an AppServer derived class when
///
/// -  an IFC triggers with ServiceName containing a host name of the form
///    &lt;service_name&gt;.&lt;homedomain&gt;
/// -  a request is received for a dialog where the service previously called
///    add_to_dialog.
///
class AppServer
{
public:
  /// Virtual destructor.
  virtual ~AppServer() {}

  /// Called when the system determines the service should be invoked for a
  /// received request.  The AppServer can either return NULL indicating it
  /// does not want to process the request, or create a suitable object
  /// derived from the AppServerTsx class to process the request.
  ///
  /// @param  helper        - The service helper to use to perform
  ///                         the underlying service-related processing.
  /// @param  req           - The received request message.
  virtual AppServerTsx* get_app_tsx(AppServerTsxHelper* helper,
                                    pjsip_msg* req) = 0;

  /// Returns the name of this service.
  const std::string service_name() { return _service_name; }

protected:
  /// Constructor.
  AppServer(const std::string& service_name) :
    _service_name(service_name) {}

private:
  /// The name of this service.
  const std::string _service_name;

};


/// The AppServerTsx class is an abstract base class used to handle the
/// application-server-specific processing of a single transaction.  It
/// encapsulates an AppServerTsxHelper, which it calls through to to perform
/// the underlying service-related processing.
///
class AppServerTsx
{
public:
  /// Virtual destructor.
  virtual ~AppServerTsx() {}

  /// Called for an initial request (dialog-initiating or out-of-dialog) with
  /// the original received request for the transaction.  Unless the reject
  /// method is called, on return from this method the request will be
  /// forwarded to all targets added using the add_target API, or to the
  /// existing RequestURI if no targets were added.
  ///
  /// @param req           - The received initial request.
  virtual void on_initial_request(pjsip_msg* req) = 0;

  /// Called for an in-dialog request with the original received request for
  /// the transaction.  Unless the reject method is called, on return from
  /// this method the request will be forwarded within the dialog.
  ///
  /// @param req           - The received in-dialog request.
  virtual void on_in_dialog_request(pjsip_msg* req) {}

  /// Called with all responses received on the transaction.  If a transport
  /// error or transaction timeout occurs on a downstream leg, this method is
  /// called with a 408 response.  The return value indicates whether the 
  /// response should be forwarded upstream (after suitable consolidation if
  /// the request was forked).  If the return value is false and new targets
  /// have been added with the add_target API, the original request is forked
  /// to them.
  ///
  /// @returns             - true if the response should be forwarded upstream
  ///                        false if the response should be dropped
  /// @param  rsp          - The received request.
  /// @param  fork_id      - The identity of the downstream fork on which
  ///                        the response was received.
  virtual bool on_response(pjsip_msg* rsp, int fork_id) {return true;}

  /// Called if the original request is cancelled (either by a received
  /// CANCEL request or an error on the inbound transport).  On return from 
  /// this method the transaction (and any remaining downstream legs) will be
  /// cancelled automatically.  No further methods will be called for this
  /// transaction.
  ///
  /// @param  status_code  - Indicates the reason for the cancellation 
  ///                        (487 for a CANCEL, 408 for a transport error
  ///                        or transaction timeout)
  virtual void on_cancel(int status_code) {}

protected:
  /// Constructor.
  AppServerTsx(AppServerTsxHelpersx* helper) : _helper(helper) {}

  /// Adds the service to the underlying SIP dialog with the specified dialog
  /// identifier.
  ///
  /// @param  dialog_id    - The dialog identifier to be used for this service.
  ///                        If omitted, a default unique identifier is created
  ///                        using parameters from the SIP request.
  ///
  void add_to_dialog(const std::string& dialog_id="")
    {_helper->add_to_dialog(dialog_id);}

  /// Returns the dialog identifier for this service.
  ///
  /// @returns             - The dialog identifier attached to this service,
  ///                        either by this ServiceTsx instance
  ///                        or by an earlier transaction in the same dialog.
  const std::string& dialog_id() const
    {return _helper->dialog_id();}

  /// Clones the request.  This is typically used when forking a request if
  /// different request modifications are required on each fork.
  ///
  /// @returns             - The cloned request message.
  /// @param  req          - The request message to clone.
  pjsip_msg* clone_request(pjsip_msg* req)
    {return _helper->clone_request(req);}

  /// Adds the specified URI as a new target for the request.  If no request
  /// is specified, the originally received request is used.  Each target is
  /// assigned a unique fork identifier, which is passed in with any subsequent
  /// received responses.
  ///
  /// @returns             - The identity of this fork.
  /// @param  request_uri  - The URI for the new target.
  /// @param  req          - The request message to use for this fork.  If NULL
  ///                        the original request message is used.
  int add_target(pjsip_uri* request_uri,
                 pjsip_msg* req=NULL)
    {return }

  /// Rejects the original request with the specified status code and text.
  /// This method can only be called when handling the original request.
  /// Any subsequent rejection of the request must be done by sending a final
  /// response using the send_response method.
  ///
  /// @param  status_code  - The SIP status code to send on the response.
  /// @param  status_text  - The SIP status text to send on the response.  If 
  ///                        omitted, the default status text for the code is
  ///                        used (if this is a standard SIP status code).
  void reject(int status_code,
              const std::string& status_text="")
    {return _helper->reject(status_code, status_text);}

  /// Sends a provisional or final response to the transaction.  If a final
  /// response is sent on an INVITE transaction that was forked, all forks 
  /// which have not yet responded are cancelled.
  ///
  /// @param  rsp          - The response message to send.
  void send_response(pjsip_msg* rsp)
    {return _helper->send_response(rsp);}

  /// Frees the specified message.  Received responses or messages that have
  /// been cloned with add_target are owned by the AppServerTsx.  It must
  /// call into ServiceTsx either to send them on or to free them (via this
  /// API).
  ///
  /// @param  msg          - The message to free.
  void free_msg(pjsip_msg* msg)
    {return _helper->free_msg(msg);}

  /// Returns the pool corresponding to a message.  This pool can then be used
  /// to allocate further headers or bodies to add to the message.
  ///
  /// @returns             - The pool corresponding to this message.
  /// @param  msg          - The message.
  pj_pool_t* get_pool(const pjsip_msg* msg)
    {return _helper->get_pool(msg);}

  /// Returns the SAS trail identifier that should be used for any SAS events
  /// related to this service invocation.
  SAS::TrailId trail() const
    {return _helper->trail();}

private:
  /// Transaction context to use for underlying service-related processing.
  ServiceTsx* _helper;

};

#endif
