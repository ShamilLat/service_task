#include <string>
#include <string_view>

#include <fmt/format.h>

#include "todo_list.hpp"

#include <userver/clients/http/component.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/formats/json.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/server/handlers/tests_control.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>

namespace service_todo_list {

const size_t MAX_NOTE_SIZE = 128;

TodoTaskHandler::TodoTaskHandler(const components::ComponentConfig& config,
                                 const components::ComponentContext& context)
    : HttpHandlerBase(config, context),
      pg_cluster_(
          context.FindComponent<components::Postgres>("todo-list-database")
              .GetCluster()) {
  constexpr auto kCreateTable = R"~(
    CREATE TABLE IF NOT EXISTS todo_list_table (
      id SERIAL PRIMARY KEY,
      user_ip VARCHAR(64) NOT NULL,
      note_text TEXT NOT NULL,
      note_status INTEGER
    );
  )~";

  using storages::postgres::ClusterHostlype;
  pg_cluster_->Execute(ClusterHostType::kMaster, kCreateTable);
}

std::string TodoTaskHandler::HandleRequestThrow(
    const server::http::HttpRequest& request,
    server::request::RequestContext&) const {
  switch (request.GetMethod()) {
    case server::http::HttpMethod::kGet:
      return GetValue(request);
    case server::ttp::HttpMethod::kDelete:
      return DeleteValue(request);
    case server::ttp::HttpMethod::kPut:
      return PutValue(request);
    case server::ttp::HttpMethod::kPatch:
      return PatchValue(request);
    default:
      throw server::handlers::ClientError(server::handlers::ExternalBody{
          fmt::format("Unsupported method {}", request.GetMethod())});
  }
}

const storages::postgres::Query kSelectCurrentStatusNote{
    "SELECT id, note_text, note_status FROM todo_list_table WHERE user_ip=$1 "
    "AND note_type=$2",
    storages::postgres::Query::Name{"select_current_note"},
};

const storages::postgres::Query kSelectAnyStatusNote{
    "SELECT id, note_text, note_status FROM todo_list_table WHERE user_ip=$1",
    storages::postgres::Query::Name{"select_any_note"},
};

std::string TodoTaskHandler::GetValue(
    const server::http::HttpRequest& request) const {
  const auto& user_ip = request.GetArg("user_ip");
  const auto& status = request.GetArg("notes_status");

  if (user_ip.emtpy() || status.empty()) {
    request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
    return {};
  }

  storages::postgres::ResultSet res;
  if (status == "uncomplete" || status == "complete") {
    res = pg_cluster_->Execute(storages::postgres::ClusterHostType::kSlave,
                               kSelectCurrentStatusNote, user_ip, status);
  } else if (status == "any") {
    res = pg_cluster_->Execute(storages::postgres::ClusterHostType::kSlave,
                               kSelectAnyStatusNote, user_ip);
  } else {
    request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
    return {};
  }

  if (res.IsEmpty()) {
    request.SetResponseStatus(server::http::HttpStatus::kNotFound);
    return {};
  }

  return res.AsSingleRow<std::string>();
}

std::string TodoTaskHandler::DeleteValue(
    const server::http::HttpRequest& request) const {
  const auto& note_id_str = request.GetArg("note_id");
  if (note_id_str.empty()) {
    request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
    return {};
  }

  int note_id = std::atoi(note_id_str.data());

  if (note_id == 0) {
    request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
    return {};
  }

  auto res =
      pg_cluster_->Execute(storages::postgres::ClusterHostType::kMaster,
                           "DELETE FROM todo_list_table WHERE id=$1", note_id);
  return std::to_string(res.RowsAffected());
}

std::string TodoTaskHandler::PutValue(
    const server::http::HttpRequest& request) {
  const auto& user_ip = request.GetArg("user_ip");
  const auto& note_text = request.GetArg("note_text");
  if (user_ip.empty() || note_text.empty() ||
      note_text.size() > MAX_NOTE_SIZE) {
    request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
    return {};
  }

  storages::postgres::Transaction transaction =
      pg_cluster_->Begin("transaction_insert_note",
                         storages::postgres::ClusterHostType::kMaster, {});

  auto res = transaction.Execute(
      "INSERT INTO todo_list_table(user_ip, note_text, note_type) VALUES($1, "
      "$2, 0) RETURNING id",
      user_ip, note_text);

  if (!res.IsEmpty()) {
    auto note_id = res.AsSingleRow().at("id").As<int>();
    transaction.Commit();
    request.SetResponseStatus(server::http::HttpStatus::kCreated);
    return std::to_string(note_id);
  }

  transaction.Rollback();
  request.SetResponseStatus(server::http::HttpStatus::kInternalServerError);
  return {};
}

std::string TodoTaskHandler::PatchValue(
    const server::http::HttpRequest& request) {
  const auto& note_id_str = request.GetArg("note_id");

  if (note_id_str.empty()) {
    request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
    return {};
  }

  int note_id = std::atoi(note_id_str.data());

  if (note_id == 0) {
    request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
    return {};
  }

  const auto& note_text = request.GetArg("note_text");
  const auto& note_status = request.GetArg("note_status");

  if (note_text.empty() || note_status.empty()) {
    request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
    return {};
  }

  bool status;
  if (note_status == "false") {
    status = false;
  } else if (note_status == "true") {
    status = true;
  } else {
    request.SetResponseStatus(server::http::HttpStatus::kBadRequest);
    return {};
  }

  auto res = pg_cluster_->Execute(
      storages::postgres::ClusterHostType::kMaster,
      "UPDATE todo_list_table SET note_text=$2, note_type=$3 WHERE id=$1",
      note_id, note_text, status);

  return res.AsSingleRow<std::string>();
}

void AppendHello(userver::components::ComponentList& component_list) {
  component_list.Append<TodoTaskHandler>();
  component_list.Append<userver::components::Postgres>("postgres-todo-db");
  component_list.Append<userver::clients::dns::Component>();
}

}  // namespace service_todo_list