use std::collections::HashMap;

use crate::error::{ServerError, ServerResult};
use crate::jobs::job_manager::job_manager;
use crate::jobs::job_types::JobStatus;
use crate::server::requests::HttpRequest;
use crate::server::response::{HttpResponse, JsonResponseBuilder};

pub fn handle_job_submit(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let route = req.require_query_param("route")?;
    // Build params by copying all query params except 'route'
    let mut params = HashMap::new();
    for (k, v) in req.query_params.iter() {
        if k != "route" { params.insert(k.clone(), v.clone()); }
    }
    let id = job_manager().submit(route, params)?;
    Ok(JsonResponseBuilder::new(202).field("job_id", id).build())
}

pub fn handle_job_status(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let id = req.require_query_param("id")?;
    if let Some(j) = job_manager().status(id) {
        let mut b = JsonResponseBuilder::new(200)
            .field("id", j.id)
            .field("route", j.route)
            .field("status", format_status(&j.status))
            .field_num("submitted_at", j.submitted_at);
        if let Some(s) = j.started_at { b = b.field_num("started_at", s); }
        if let Some(f) = j.finished_at { b = b.field_num("finished_at", f); }
        if let Some(rs) = j.result_status { b = b.field_num("result_status", rs as u64); }
        if let Some(err) = j.error { b = b.field("error", err); }
        Ok(b.build())
    } else {
        Err(ServerError::not_found(id))
    }
}

pub fn handle_job_result(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let id = req.require_query_param("id")?;
    if let Some(j) = job_manager().status(id) {
        match j.result_raw {
            Some(bytes) => Ok(HttpResponse::ok().with_text(String::from_utf8_lossy(&bytes).to_string())),
            None => Err(ServerError::invalid_param("id", "result not ready")),
        }
    } else {
        Err(ServerError::not_found(id))
    }
}

pub fn handle_job_cancel(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let id = req.require_query_param("id")?;
    let ok = job_manager().cancel(id);
    let status = if ok { 200 } else { 404 };
    Ok(JsonResponseBuilder::new(status).field("id", id).field_bool("canceled", ok).build())
}

pub fn handle_job_list(_req: &HttpRequest) -> ServerResult<HttpResponse> {
    let jobs = job_manager().list();
    let mut arr = String::from("[");
    for (i, j) in jobs.iter().enumerate() {
        if i>0 { arr.push(','); }
        arr.push_str(&format!(
            r#"{{"id":"{}","route":"{}","status":"{}"}}"#,
            j.id, j.route, format_status(&j.status)
        ));
    }
    arr.push(']');
    Ok(JsonResponseBuilder::new(200).field_raw("jobs", arr).build())
}

fn format_status(s: &JobStatus) -> &'static str {
    match s {
        JobStatus::Queued => "queued",
        JobStatus::Running => "running",
        JobStatus::Done => "done",
        JobStatus::Failed => "failed",
        JobStatus::Canceled => "canceled",
    }
}
