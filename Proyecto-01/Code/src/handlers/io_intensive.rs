//! IO-intensive endpoint handlers (skeleton)
//!
//! Parameter parsing + worker offload. Actual IO ops live in `io_operations`.

use crate::error::ServerResult;
use crate::handlers::handler_traits::QueryParamExt;
use crate::server::requests::HttpRequest;
use crate::server::response::{HttpResponse, JsonResponseBuilder};
use crate::workers::worker_manager::worker_manager;

fn not_implemented(endpoint: &str) -> HttpResponse {
    JsonResponseBuilder::new(501)
        .field("endpoint", endpoint)
        .field("status", "not_implemented")
        .build()
}

/// GET /sortfile?name=FILE&algo=merge|quick
pub fn handle_sortfile(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let name = req.require_query_param("name")?;
    let algo: String = req.parse_param_or("algo", String::from("merge"))?;
    let name = name.to_string();
    let resp = worker_manager().submit_io(move || {
        JsonResponseBuilder::new(501)
            .field("filename", &name)
            .field("algo", &algo)
            .field("endpoint", "/sortfile")
            .field("status", "not_implemented")
            .build()
    })?;
    Ok(resp)
}

/// GET /wordcount?name=FILE
pub fn handle_wordcount(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let name = req.require_query_param("name")?.to_string();
    let resp = worker_manager().submit_io(move || {
        JsonResponseBuilder::new(501)
            .field("filename", &name)
            .field("endpoint", "/wordcount")
            .field("status", "not_implemented")
            .build()
    })?;
    Ok(resp)
}

/// GET /grep?name=FILE&pattern=REGEX
pub fn handle_grep(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let name = req.require_query_param("name")?.to_string();
    let pattern = req.require_query_param("pattern")?.to_string();
    let resp = worker_manager().submit_io(move || {
        JsonResponseBuilder::new(501)
            .field("filename", &name)
            .field("pattern", &pattern)
            .field("endpoint", "/grep")
            .field("status", "not_implemented")
            .build()
    })?;
    Ok(resp)
}

/// GET /compress?name=FILE&codec=gzip|xz
pub fn handle_compress(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let name = req.require_query_param("name")?.to_string();
    let codec: String = req.parse_param_or("codec", String::from("gzip"))?;
    let resp = worker_manager().submit_io(move || {
        JsonResponseBuilder::new(501)
            .field("filename", &name)
            .field("codec", &codec)
            .field("endpoint", "/compress")
            .field("status", "not_implemented")
            .build()
    })?;
    Ok(resp)
}

/// GET /hashfile?name=FILE&algo=sha256
pub fn handle_hashfile(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let name = req.require_query_param("name")?.to_string();
    let algo: String = req.parse_param_or("algo", String::from("sha256"))?;
    let resp = worker_manager().submit_io(move || {
        JsonResponseBuilder::new(501)
            .field("filename", &name)
            .field("algo", &algo)
            .field("endpoint", "/hashfile")
            .field("status", "not_implemented")
            .build()
    })?;
    Ok(resp)
}
