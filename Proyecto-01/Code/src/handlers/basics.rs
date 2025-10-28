//! Basic endpoint handlers
//!
//! Implements simple endpoints like /timestamp, /reverse, /toupper, etc.

use crate::error::{ServerError, ServerResult};
use crate::handlers::handler_traits::QueryParamExt;
use crate::server::requests::HttpRequest;
use crate::server::response::{HttpResponse, JsonResponseBuilder};
use crate::utils::crypto;
use std::time::{SystemTime, UNIX_EPOCH};

/// Handle /timestamp - Return current Unix timestamp
pub fn handle_timestamp(_req: &HttpRequest) -> ServerResult<HttpResponse> {
    let timestamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs();

    let response = JsonResponseBuilder::new(200)
        .field_num("timestamp", timestamp)
        .field("unit", "seconds")
        .build();

    Ok(response)
}

/// Handle /reverse?text=... - Reverse a string
pub fn handle_reverse(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let text = req.require_query_param("text")?;
    let reversed: String = text.chars().rev().collect();

    let response = JsonResponseBuilder::new(200)
        .field("original", text)
        .field("reversed", &reversed)
        .build();

    Ok(response)
}

/// Handle /toupper?text=... - Convert to uppercase
pub fn handle_toupper(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let text = req.require_query_param("text")?;
    let upper = text.to_uppercase();

    let response = JsonResponseBuilder::new(200)
        .field("original", text)
        .field("upper", &upper)
        .build();

    Ok(response)
}

/// Handle /hash?text=... - Calculate SHA256 hash
pub fn handle_hash(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let text = req.require_query_param("text")?;
    let hash = crypto::sha256_hex(text.as_bytes());

    let response = JsonResponseBuilder::new(200)
        .field("text", text)
        .field("sha256", &hash)
        .build();

    Ok(response)
}

/// Handle /random?count=N&min=A&max=B - Generate random numbers
pub fn handle_random(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let count: usize = req.parse_param("count")?;
    let min: i64 = req.parse_param_or("min", 0)?;
    let max: i64 = req.parse_param_or("max", 100)?;

    if count > 10000 {
        return Err(ServerError::invalid_param(
            "count",
            "maximum is 10000",
        ));
    }

    if min >= max {
        return Err(ServerError::invalid_param(
            "min/max",
            "min must be less than max",
        ));
    }

    let numbers = crypto::generate_random_numbers(count, min, max);
    let numbers_json = format!(
        "[{}]",
        numbers
            .iter()
            .map(|n| n.to_string())
            .collect::<Vec<_>>()
            .join(",")
    );

    let response = JsonResponseBuilder::new(200)
        .field_num("count", count)
        .field_num("min", min)
        .field_num("max", max)
        .field_raw("numbers", numbers_json)
        .build();

    Ok(response)
}

/// Handle /help - List all available endpoints
pub fn handle_help(_req: &HttpRequest) -> ServerResult<HttpResponse> {
    let endpoints = vec![
        "/timestamp - Get current Unix timestamp",
        "/reverse?text=... - Reverse a string",
        "/toupper?text=... - Convert to uppercase",
        "/hash?text=... - Calculate SHA256 hash",
        "/random?count=N&min=A&max=B - Generate random numbers",
        "/fibonacci?num=N - Calculate Fibonacci number",
        "/createfile?name=...&content=...&repeat=N - Create a file",
        "/deletefile?name=... - Delete a file",
        "/status - Get server status",
        "/help - This help message",
    ];

    let endpoints_json = format!(
        "[{}]",
        endpoints
            .iter()
            .map(|s| format!(r#""{}""#, s))
            .collect::<Vec<_>>()
            .join(",")
    );

    let response = JsonResponseBuilder::new(200)
        .field("server", "HTTP/1.0 Server")
        .field_raw("endpoints", endpoints_json)
        .build();

    Ok(response)
}

/// Handle /fibonacci?num=N - Calculate Fibonacci number
pub fn handle_fibonacci(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let n: u64 = req.parse_param("num")?;

    if n > 90 {
        return Err(ServerError::invalid_param(
            "num",
            "maximum is 90 (overflow protection)",
        ));
    }

    let start = SystemTime::now();
    let result = fibonacci(n);
    let elapsed = start.elapsed().unwrap_or_default().as_millis();

    let response = JsonResponseBuilder::new(200)
        .field_num("n", n)
        .field_num("result", result)
        .field_num("elapsed_ms", elapsed)
        .build();

    Ok(response)
}

/// Calculate Fibonacci number (iterative)
fn fibonacci(n: u64) -> u64 {
    if n == 0 {
        return 0;
    }
    if n == 1 {
        return 1;
    }

    let mut a = 0u64;
    let mut b = 1u64;

    for _ in 2..=n {
        let temp = a.wrapping_add(b);
        a = b;
        b = temp;
    }

    b
}

/// Handle /status - Server status
pub fn handle_status(_req: &HttpRequest) -> ServerResult<HttpResponse> {
    let uptime = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs();

    let pid = std::process::id();

    let response = JsonResponseBuilder::new(200)
        .field_num("pid", pid)
        .field_num("uptime_seconds", uptime)
        .field("status", "running")
        .build();

    Ok(response)
}

/// Handle /createfile?name=...&content=...&repeat=N
pub fn handle_createfile(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let name = req.require_query_param("name")?;
    let content = req.require_query_param("content")?;
    let repeat: usize = req.parse_param_or("repeat", 1)?;

    // Validate filename (no path traversal)
    if name.contains("..") || name.contains('/') || name.contains('\\') {
        return Err(ServerError::invalid_param(
            "name",
            "invalid filename",
        ));
    }

    if repeat > 100000 {
        return Err(ServerError::invalid_param(
            "repeat",
            "maximum is 100000",
        ));
    }

    let full_content = content.repeat(repeat);
    let file_path = format!("./data/{}", name);

    // Create data directory if it doesn't exist
    std::fs::create_dir_all("./data")
        .map_err(|e| ServerError::FileOperation(format!("Failed to create data directory: {}", e)))?;

    std::fs::write(&file_path, full_content.as_bytes())
        .map_err(|e| ServerError::FileOperation(format!("Failed to write file: {}", e)))?;

    let response = JsonResponseBuilder::new(200)
        .field("filename", name)
        .field_num("size_bytes", full_content.len())
        .field("status", "created")
        .build();

    Ok(response)
}

/// Handle /deletefile?name=...
pub fn handle_deletefile(req: &HttpRequest) -> ServerResult<HttpResponse> {
    let name = req.require_query_param("name")?;

    // Validate filename
    if name.contains("..") || name.contains('/') || name.contains('\\') {
        return Err(ServerError::invalid_param(
            "name",
            "invalid filename",
        ));
    }

    let file_path = format!("./data/{}", name);

    if !std::path::Path::new(&file_path).exists() {
        return Err(ServerError::not_found(&format!("File not found: {}", name)));
    }

    std::fs::remove_file(&file_path)
        .map_err(|e| ServerError::FileOperation(format!("Failed to delete file: {}", e)))?;

    let response = JsonResponseBuilder::new(200)
        .field("filename", name)
        .field("status", "deleted")
        .build();

    Ok(response)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_fibonacci() {
        assert_eq!(fibonacci(0), 0);
        assert_eq!(fibonacci(1), 1);
        assert_eq!(fibonacci(10), 55);
        assert_eq!(fibonacci(20), 6765);
    }
}