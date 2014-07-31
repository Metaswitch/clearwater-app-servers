/**
 * @file appserver.h  Abstract SIP Application Server interface definition
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

#ifndef APPSERVER_H__
#define APPSERVER_H__

extern "C" {
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <stdint.h>
}

#include "sas.h"

class ServiceTsxHelper;
class AppServerTsxHelper;
class AppServer;
class AppServerTsx;


/// The AppServerTsxHelper class handles the underlying service-related
/// processing of a single transaction for an AppServer.  Once a service has
/// been triggered as part of handling a transaction, the related
/// AppServerTsxHelper is inspected to determine what should be done next, e.g.
/// forward the request, reject it, fork it etc.
/// 
/// This is an abstract base class to allow for alternative implementations -
/// in particular, production and test.  It is implemented by the underlying
/// service infrastructure, not by the services themselves.
///
class AppServerTsxHelper
{
public:
  /// Virtual destructor.
  virtual ~AppServerTsxHelper() {}

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
  /// different request modifications are required on each fork or for storing
  /// off to handle late forking.
  ///
  /// @returns             - The cloned request message.
  /// @param  req          - The request message to clone.
  virtual pjsip_msg* clone_request(pjsip_msg* req) = 0;

  /// Create a response from a given request, this response can be passed to
  /// send_response or stored for later.  It may be freed again by passing
  /// it to free_message.
  ///
  /// @returns             - The new response message.
  /// @param  req          - The request to build a response for.
  /// @param  status_code  - The SIP status code for the response.
  /// @param  status_text  - The text part of the status line.
  virtual pjsip_msg* create_response(pjsip_msg* req,
                                     pjsip_status_code status_code,
                                     const std::string& status_text="") = 0;

  /// Indicate that the request should be forwarded following standard routing
  /// rules.  Note that, even if other Route headers are added by this AS, the
  /// request will be routed back to the S-CSCF that sent the request in the
  /// first place after all those routes have been visited.
  ///
  /// This function may be called repeatedly to create downstream forks of an
  /// original upstream request and may also be called during response processing
  /// or an original request to create a late fork.  When processing an in-dialog
  /// request this function may only be called once.
  /// 
  /// This function may be called while processing initial requests,
  /// in-dialog requests and cancels but not during response handling.
  ///
  /// @returns             - The ID of this forwarded request
  /// @param  req          - The request message to use for forwarding.
  virtual int send_request(pjsip_msg*& req) = 0;

  /// Indicate that the response should be forwarded following standard routing
  /// rules.  Note that, if this service created multiple forks, the responses
  /// will be aggregated before being sent downstream.
  /// 
  /// This function may be called while handling any response.
  ///
  /// @param  rsp          - The response message to use for forwarding.
  virtual void send_response(pjsip_msg*& rsp) = 0;

  /// Frees the specified message.  Received responses or messages that have
  /// been cloned with add_target are owned by the AppServerTsx.  It must
  /// call into ServiceTsx either to send them on or to free them (via this
  /// API).
  ///
  /// @param  msg          - The message to free.
  virtual void free_msg(pjsip_msg*& msg) = 0;

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
  AppServerTsxHelper() {};
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
  /// the original received request for the transaction.
  ///
  /// During this function, the implementation must either call send_request to
  /// forward a request upstream, or call send_response to send a final response.
  /// In either case, the implmentation may call send_response with any number of
  /// non-final responses to be forwarded upstream.
  ///
  /// @param req           - The received initial request.
  virtual void on_initial_request(pjsip_msg* req) { send_request(req); }

  /// Called for an in-dialog request with the original received request for
  /// the transaction.
  ///
  /// During this function, exactly one of the following functions must be called, 
  /// otherwise the request will be rejected with a 503 Server Internal
  /// Error:
  ///
  /// * send_request()
  /// * send_response()
  ///
  /// @param req           - The received in-dialog request.
  virtual void on_in_dialog_request(pjsip_msg* req) { send_request(req); }

  /// Called with all responses received on the transaction.  If a transport
  /// error or transaction timeout occurs on a downstream leg, this method is
  /// called with a 408 response.
  ///
  /// During this function, exactly one of the following functions must be called, 
  /// otherwise the request will be rejected with a 503 Server Internal
  /// Error:
  ///
  /// * send_response() - Multiple final responses will be aggregated automatically
  ///                     across forks.
  /// * send_request()
  ///
  /// @param  rsp          - The received request.
  /// @param  fork_id      - The identity of the downstream fork on which
  ///                        the response was received.
  virtual void on_response(pjsip_msg* rsp, int fork_id) { send_response(rsp); }

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
  AppServerTsx(AppServerTsxHelper* helper) : _helper(helper) {}

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

  /// Create a response from a given request, this response can be passed to
  /// send_response or stored for later.  It may be freed again by passing
  /// it to free_message.
  ///
  /// @returns             - The new response message.
  /// @param  req          - The request to build a response for.
  /// @param  status_code  - The SIP status code for the response.
  /// @param  status_text  - The text part of the status line.
  virtual pjsip_msg* create_response(pjsip_msg* req,
                                     pjsip_status_code status_code,
                                     const std::string& status_text="")
    {return _helper->create_response(req, status_code, status_text);}

  /// Indicate that the request should be forwarded following standard routing
  /// rules.  Note that, even if other Route headers are added by this AS, the
  /// request will be routed back to the S-CSCF that sent the request in the
  /// first place after all those routes have been visited.
  ///
  /// This function may be called repeatedly to create downstream forks of an
  /// original upstream request and may also be called during response processing
  /// or an original request to create a late fork.  When processing an in-dialog
  /// request this function may only be called once.
  /// 
  /// This function may be called while processing initial requests,
  /// in-dialog requests and cancels but not during response handling.
  ///
  /// @returns             - The ID of this forwarded request
  /// @param  req          - The request message to use for forwarding.
  int send_request(pjsip_msg*& req)
    {return _helper->send_request(req);}

  /// Indicate that the response should be forwarded following standard routing
  /// rules.  Note that, if this service created multiple forks, the responses
  /// will be aggregated before being sent downstream.
  /// 
  /// This function may be called while handling any response.
  ///
  /// @param  rsp          - The response message to use for forwarding.
  void send_response(pjsip_msg*& rsp)
    {_helper->send_response(rsp);}

  /// Frees the specified message.  Received responses or messages that have
  /// been cloned with add_target are owned by the AppServerTsx.  It must
  /// call into ServiceTsx either to send them on or to free them (via this
  /// API).
  ///
  /// @param  msg          - The message to free.
  void free_msg(pjsip_msg*& msg)
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
  AppServerTsxHelper* _helper;

};

#endif
