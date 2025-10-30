//! CPU-intensive endpoint handlers (skeleton)
//!
//! This file provides parameter parsing and worker offloading only.
//! The underlying algorithms will be implemented in `crate::algorithms::*`.

use crate::error::{ServerError, ServerResult};
use crate::handlers::handler_traits::QueryParamExt;
use crate::server::requests::HttpRequest;
use crate::server::response::{HttpResponse, JsonResponseBuilder};
use crate::workers::worker_manager::worker_manager;
<<<<<<< HEAD
use crate::workers::worker_types::WorkPriority;
=======
>>>>>>> 3b0a5ce33d497ca984a7cc0bae834e04c8fc21e1
use crate::algorithms::{prime, mandelbrot, matrix_ops, pi_calculation};
use crate::utils::validation::validate_filename;
use std::time::SystemTime;

fn not_implemented(endpoint: &str) -> HttpResponse {
    JsonResponseBuilder::new(501)
        .field("endpoint", endpoint)
        .field("status", "not_implemented")
        .build()
}

/// GET /isprime?n=NUM[&algo=division|mr][&rounds=6]
pub fn handle_isprime(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let n: u64 = req.parse_param("n")?;
    // Default to Miller–Rabin; allow algo=division
    let algo: String = req.parse_param_or("algo", String::from("mr"))?;
    let rounds: u32 = req.parse_param_or("rounds", 6)?;

<<<<<<< HEAD
    let prio = WorkPriority::from_str(req.query_params.get("prio").map(|s| s.as_str()).unwrap_or("normal"));
    let timeout = worker_manager().cpu_timeout();
    let resp = worker_manager().submit_for_with_priority("/isprime", timeout, prio, move || {
=======
    let resp = worker_manager().submit_cpu(move || {
>>>>>>> 3b0a5ce33d497ca984a7cc0bae834e04c8fc21e1
        let start = SystemTime::now();
        let (algo_used, is_p) = match algo.to_lowercase().as_str() {
            "division" => ("division", prime::is_prime_trial(n)),
            _ => ("mr", prime::is_prime_mr(n, rounds)),
        };
        let elapsed = start.elapsed().unwrap_or_default().as_millis();

        JsonResponseBuilder::new(200)
            .field_num("n", n)
            .field("algo", algo_used)
            .field_num("rounds", rounds)
            .field_bool("is_prime", is_p)
            .field_num("elapsed_ms", elapsed)
            .build()
    })?;
    Ok(resp)
}

/// GET /factor?n=NUM
pub fn handle_factor(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let n: u64 = req.parse_param("n")?;
<<<<<<< HEAD
    let prio = WorkPriority::from_str(req.query_params.get("prio").map(|s| s.as_str()).unwrap_or("normal"));
    let timeout = worker_manager().cpu_timeout();
    let resp = worker_manager().submit_for_with_priority("/factor", timeout, prio, move || {
=======
    let resp = worker_manager().submit_cpu(move || {
>>>>>>> 3b0a5ce33d497ca984a7cc0bae834e04c8fc21e1
        let start = SystemTime::now();
        let factors = prime::factor_trial(n);
        let elapsed = start.elapsed().unwrap_or_default().as_millis();

        // Build JSON array of objects: [{"p":2,"count":3}, ...]
        let list = factors
            .iter()
            .map(|(p, c)| format!(r#"{{"p":{},"count":{}}}"#, p, c))
            .collect::<Vec<_>>()
            .join(",");
        let json = format!("[{}]", list);

        JsonResponseBuilder::new(200)
            .field_num("n", n)
            .field_raw("factors", json)
            .field_num("elapsed_ms", elapsed)
            .build()
    })?;
    Ok(resp)
}

/// GET /pi?digits=D
pub fn handle_pi(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let digits: u32 = req.parse_param("digits")?;
    let algo: String = req.parse_param_or("algo", String::from("spigot"))?;

    // Guardrails per algorithm
    match algo.as_str() {
        "spigot" => {
            if digits > 5000 {
                return Err(ServerError::invalid_param("digits", "maximum is 5000 for spigot"));
            }
        }
        "chudnovsky" => {
            if digits > 15 {
                return Err(ServerError::invalid_param("digits", "maximum is 15 for chudnovsky (f64 precision)"));
            }
        }
        _ => return Err(ServerError::invalid_param("algo", "use spigot or chudnovsky")),
    }

    let prio = WorkPriority::from_str(req.query_params.get("prio").map(|s| s.as_str()).unwrap_or("normal"));
    let timeout = worker_manager().cpu_timeout();
    let resp = worker_manager().submit_for_with_priority("/pi", timeout, prio, move || {
        let start = SystemTime::now();
        let (algo_used, value) = match algo.as_str() {
            "chudnovsky" => ("chudnovsky", pi_calculation::pi_chudnovsky_string(digits)),
            _ => ("spigot", pi_calculation::pi_spigot_string(digits)),
        };
        let elapsed = start.elapsed().unwrap_or_default().as_millis();
        JsonResponseBuilder::new(200)
            .field_num("digits", digits)
            .field("algo", algo_used)
            .field("value", value)
            .field_num("elapsed_ms", elapsed)
            .build()
    })?;
    Ok(resp)
}

/// GET /mandelbrot?width=W&height=H&max_iter=I[&dump=ppm]
pub fn handle_mandelbrot(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let width: u32 = req.parse_param("width")?;
    let height: u32 = req.parse_param("height")?;
    let max_iter: u32 = req.parse_param_or("max_iter", 1000)?;
    let dump: Option<String> = req.parse_param_optional("dump")?;
<<<<<<< HEAD
    let prio = WorkPriority::from_str(req.query_params.get("prio").map(|s| s.as_str()).unwrap_or("normal"));
    let timeout = worker_manager().cpu_timeout();
    let resp = worker_manager().submit_for_with_priority("/mandelbrot", timeout, prio, move || {
=======
    let resp = worker_manager().submit_cpu(move || {
>>>>>>> 3b0a5ce33d497ca984a7cc0bae834e04c8fc21e1
        let start = SystemTime::now();
        let map = mandelbrot::mandelbrot_iterations(width, height, max_iter);
        let elapsed = start.elapsed().unwrap_or_default().as_millis();
        let rows_json = format!(
            "[{}]",
            map.iter()
                .map(|row| {
                    let s = row.iter().map(|v| v.to_string()).collect::<Vec<_>>().join(",");
                    format!("[{}]", s)
                })
                .collect::<Vec<_>>()
                .join(",")
        );

        // Optionally dump to PGM file
        let mut builder = JsonResponseBuilder::new(200)
            .field_num("width", width)
            .field_num("height", height)
            .field_num("max_iter", max_iter)
            .field_num("elapsed_ms", elapsed)
            .field_raw("map", rows_json);

        if let Some(filename) = dump {
            // Basic filename validation (no path traversal)
            if let Err(e) = validate_filename(&filename) {
                return JsonResponseBuilder::new(400)
                    .field("error", e.to_string())
                    .build();
            }

            // Ensure data directory exists
            if let Err(e) = std::fs::create_dir_all("./data") {
                return JsonResponseBuilder::new(500)
                    .field("error", format!("Failed to create data dir: {}", e))
                    .build();
            }

            let path = format!("./data/{}", filename);

            // Determine scaling to 0..255 (points that reach max_iter -> 0)
            let mut max_seen = 0u32;
            for row in &map {
                for &v in row {
                    if v > max_seen { max_seen = v; }
                }
            }
            let scale = if max_iter > 0 { max_iter } else { 1 };

            let mut pgm = String::new();
            pgm.push_str("P2\n");
            pgm.push_str(&format!("{} {}\n", width, height));
            pgm.push_str("255\n");
            for row in &map {
                for (idx, &v) in row.iter().enumerate() {
                    let val = if v >= scale { 0 } else { ((v as u64 * 255) / scale as u64) as u32 };
                    if idx > 0 { pgm.push(' '); }
                    pgm.push_str(&val.to_string());
                }
                pgm.push('\n');
            }

            match std::fs::write(&path, pgm.as_bytes()) {
                Ok(_) => {
                    builder = builder.field("dump", path);
                }
                Err(e) => {
                    return JsonResponseBuilder::new(500)
                        .field("error", format!("Failed to write PGM: {}", e))
                        .build();
                }
            }
        }

        builder.build()
    })?;
    Ok(resp)
}

/// GET /matrixmul?size=N&seed=S
pub fn handle_matrixmul(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let size: u32 = req.parse_param("size")?;
    let seed: u64 = req.parse_param_or("seed", 0)?;
<<<<<<< HEAD
    let prio = WorkPriority::from_str(req.query_params.get("prio").map(|s| s.as_str()).unwrap_or("normal"));
    let timeout = worker_manager().cpu_timeout();
    let resp = worker_manager().submit_for_with_priority("/matrixmul", timeout, prio, move || {
=======
    let resp = worker_manager().submit_cpu(move || {
>>>>>>> 3b0a5ce33d497ca984a7cc0bae834e04c8fc21e1
        let start = SystemTime::now();
        let hash = matrix_ops::matrixmul_hash(size, seed);
        let elapsed = start.elapsed().unwrap_or_default().as_millis();
        JsonResponseBuilder::new(200)
            .field_num("size", size)
            .field_num("seed", seed)
            .field("hash", hash)
            .field_num("elapsed_ms", elapsed)
            .build()
    })?;
    Ok(resp)
}
