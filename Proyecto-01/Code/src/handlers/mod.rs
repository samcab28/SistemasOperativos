//! Request handlers module

pub mod basics;
pub mod handler_traits;

// Future sprint modules
#[allow(dead_code)]
pub mod cpu_intensive;
#[allow(dead_code)]
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