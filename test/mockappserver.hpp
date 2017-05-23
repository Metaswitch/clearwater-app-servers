/**
 * @file mockappserver.h  Mock Application Server interfaces.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCKAPPSERVER_H__
#define MOCKAPPSERVER_H__

#include "gmock/gmock.h"
#include "appserver.h"

class MockAppServerTsxHelper : public AppServerTsxHelper
{
public:
  MockAppServerTsxHelper(std::string dialog_id = "",
                         SAS::TrailId trail = 0) :
                        _dialog_id(dialog_id),
                        _trail(trail) {}

  const std::string& dialog_id() const {return _dialog_id;}
  std::string _dialog_id;

  SAS::TrailId trail() const {return _trail;}
  SAS::TrailId _trail;

  MOCK_METHOD0(original_request, pjsip_msg*());
  MOCK_CONST_METHOD0(route_hdr, const pjsip_route_hdr*());
  MOCK_METHOD1(add_to_dialog, void(const std::string&));
  MOCK_METHOD1(clone_request, pjsip_msg*(pjsip_msg*));
  MOCK_METHOD1(clone_msg, pjsip_msg*(pjsip_msg*));
  MOCK_METHOD3(create_response, pjsip_msg*(pjsip_msg*, pjsip_status_code, const std::string&));
  MOCK_METHOD1(send_request, int(pjsip_msg*&));
  MOCK_METHOD1(send_response, void(pjsip_msg*&));
  MOCK_METHOD2(cancel_fork, void(int, int));
  MOCK_METHOD1(free_msg, void(pjsip_msg*&));
  MOCK_METHOD1(get_pool, pj_pool_t*(const pjsip_msg*));
  MOCK_METHOD3(schedule_timer, bool(void*, TimerID&, int));
  MOCK_METHOD1(cancel_timer, void(TimerID));
  MOCK_METHOD1(timer_running, bool(TimerID));
};


class MockAppServer : public AppServer
{
public:
  MockAppServer(const std::string& service_name = "mock") : AppServer(service_name) {}

  MOCK_METHOD5(get_app_tsx, AppServerTsx*(SproutletHelper*, pjsip_msg*, pjsip_sip_uri*&, pj_pool_t*, SAS::TrailId));
};


class MockAppServerTsx : public AppServerTsx
{
public:
  MockAppServerTsx() : AppServerTsx() {}

  MOCK_METHOD1(on_initial_request, void(pjsip_msg*));
  MOCK_METHOD1(on_in_dialog_request, void(pjsip_msg*));
  MOCK_METHOD2(on_response, void(pjsip_msg*, int));
  MOCK_METHOD1(on_cancel, void(int));
};

#endif
