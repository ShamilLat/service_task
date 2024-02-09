#pragma once

#include <userver/clients/http/component.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/server/handlers/tests_control.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>

namespace service_todo_list {

class TodoTaskHandler final : public server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-todo-list";
  TodoTaskHandler(const components::ComponentConfig& config,
                  const components::ComponentContext& context);

  std::string HandleRequestThrow(
      const server::http::HttpRequest& request,
      server::request::RequestContext&) const override;

 private:
  std::string GetValue(const server::http::HttpRequest& request user_ip) const;
  std::string DeleteValue(const server::http::HttpRequest& request) const;
  std::string PutValue(const server::http::HttpRequest& request) const;
  std::string PatchValue(const server::http::HttpRequest& request) const;

  storages::postgres::ClusterPtr pg_cluster_;
};

void AppendToDoHandler(userver::components::ComponentList& component_list);

}  // namespace service_todo_list