//! Request handlers module

pub mod basics;
pub mod handler_traits;

// CPU/IO intensive handlers (skeletons provided)
pub mod cpu_intensive;
pub mod io_intensive;
#[allow(dead_code)]
pub mod job_endpoints;
#[allow(dead_code)]
pub mod metrics;

// Re-export handler functions
pub use basics::{
    handle_createfile, handle_deletefile, handle_fibonacci, handle_hash,
    handle_help, handle_random, handle_reverse, handle_status, handle_timestamp,
    handle_toupper,
};

pub use handler_traits::QueryParamExt;

// Re-export CPU/IO handler functions for router wiring
pub use cpu_intensive::{
    handle_factor, handle_isprime, handle_mandelbrot, handle_matrixmul, handle_pi,
};
pub use io_intensive::{
    handle_compress, handle_grep, handle_hashfile, handle_sortfile, handle_wordcount,
};

// Expose a global list of available routes for dynamic /help
use std::sync::OnceLock;
static ROUTES: OnceLock<Vec<String>> = OnceLock::new();

pub fn set_available_routes(routes: Vec<String>) {
    let _ = ROUTES.set(routes);
}

pub fn available_routes() -> &'static [String] {
    ROUTES.get().map(|v| v.as_slice()).unwrap_or(&[])
}
