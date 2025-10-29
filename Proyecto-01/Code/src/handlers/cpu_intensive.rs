//! CPU-intensive endpoint handlers (skeleton)
//!
//! This file provides parameter parsing and worker offloading only.
//! The underlying algorithms will be implemented in `crate::algorithms::*`.

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

/// GET /isprime?n=NUM[&algo=division|mr][&rounds=5]
pub fn handle_isprime(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let n: u64 = req.parse_param("n")?;
    let _algo: String = req.parse_param_or("algo", String::from("mr"))?;
    let _rounds: u32 = req.parse_param_or("rounds", 5)?;

    // Offload placeholder to CPU pool (algorithms to be implemented)
    let resp = worker_manager().submit_cpu(move || {
        JsonResponseBuilder::new(501)
            .field_num("n", n)
            .field("endpoint", "/isprime")
            .field("status", "not_implemented")
            .build()
    })?;
    Ok(resp)
}

/// GET /factor?n=NUM
pub fn handle_factor(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let n: u64 = req.parse_param("n")?;
    let resp = worker_manager().submit_cpu(move || {
        JsonResponseBuilder::new(501)
            .field_num("n", n)
            .field("endpoint", "/factor")
            .field("status", "not_implemented")
            .build()
    })?;
    Ok(resp)
}

/// GET /pi?digits=D
pub fn handle_pi(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let digits: u32 = req.parse_param("digits")?;
    let resp = worker_manager().submit_cpu(move || {
        JsonResponseBuilder::new(501)
            .field_num("digits", digits)
            .field("endpoint", "/pi")
            .field("status", "not_implemented")
            .build()
    })?;
    Ok(resp)
}

/// GET /mandelbrot?width=W&height=H&max_iter=I[&dump=ppm]
pub fn handle_mandelbrot(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let width: u32 = req.parse_param("width")?;
    let height: u32 = req.parse_param("height")?;
    let max_iter: u32 = req.parse_param_or("max_iter", 1000)?;
    let _dump: Option<String> = req.parse_param_optional("dump")?;
    let resp = worker_manager().submit_cpu(move || {
        JsonResponseBuilder::new(501)
            .field_num("width", width)
            .field_num("height", height)
            .field_num("max_iter", max_iter)
            .field("endpoint", "/mandelbrot")
            .field("status", "not_implemented")
            .build()
    })?;
    Ok(resp)
}

/// GET /matrixmul?size=N&seed=S
pub fn handle_matrixmul(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let size: u32 = req.parse_param("size")?;
    let seed: u64 = req.parse_param_or("seed", 0)?;
    let resp = worker_manager().submit_cpu(move || {
        JsonResponseBuilder::new(501)
            .field_num("size", size)
            .field_num("seed", seed)
            .field("endpoint", "/matrixmul")
            .field("status", "not_implemented")
            .build()
    })?;
    Ok(resp)
}
