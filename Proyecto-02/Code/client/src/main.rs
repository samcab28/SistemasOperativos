use std::{
    fs::File,
    io::{self, BufRead, BufReader},
    path::PathBuf,
};

use anyhow::{anyhow, Context};
use chrono::{DateTime, Utc};
use clap::{Parser, Subcommand};
use common::{EventPayload, IngestRequest, TopologySpec};
use reqwest::Client;
use uuid::Uuid;

#[derive(Parser)]
#[command(author, version, about = "CLI para el mini motor de streaming")]
struct Cli {
    #[arg(long, default_value = "http://127.0.0.1:8080")]
    master_url: String,
    #[command(subcommand)]
    command: Command,
}

#[derive(Subcommand)]
enum Command {
    SubmitTopology {
        #[arg(value_name = "FILE", help = "Ruta al archivo JSON con la topología")]
        path: PathBuf,
    },
    Status {
        #[arg(value_name = "ID")]
        id: Uuid,
    },
    Ingest {
        #[arg(value_name = "ID")]
        id: Uuid,
        #[arg(short, long, value_name = "FILE", help = "Archivo JSONL con eventos; si se omite se usa stdin")]
        file: Option<PathBuf>,
    },
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let cli = Cli::parse();
    let client = Client::new();

    match cli.command {
        Command::SubmitTopology { path } => submit_topology(&client, &cli.master_url, path).await?,
        Command::Status { id } => get_status(&client, &cli.master_url, id).await?,
        Command::Ingest { id, file } => ingest_events(&client, &cli.master_url, id, file).await?,
    }

    Ok(())
}

async fn submit_topology(
    client: &Client,
    master_url: &str,
    path: PathBuf,
) -> anyhow::Result<()> {
    // Lee el spec de topología y lo envía al master.
    let spec: TopologySpec = serde_json::from_reader(
        File::open(&path).with_context(|| format!("No se puede abrir {:?}", path))?,
    )?;
    let res: common::TopologySubmitResponse = client
        .post(format!("{}/api/v1/topologies", master_url))
        .json(&spec)
        .send()
        .await?
        .error_for_status()?
        .json()
        .await?;
    println!("Topología creada: {}", res.topology_id);
    Ok(())
}

async fn get_status(client: &Client, master_url: &str, id: Uuid) -> anyhow::Result<()> {
    // Consulta estado/métricas de la topología.
    let status: common::TopologyStatus = client
        .get(format!("{}/api/v1/topologies/{}", master_url, id))
        .send()
        .await?
        .error_for_status()?
        .json()
        .await?;
    println!("{}", serde_json::to_string_pretty(&status)?);
    Ok(())
}

async fn ingest_events(
    client: &Client,
    master_url: &str,
    id: Uuid,
    file: Option<PathBuf>,
) -> anyhow::Result<()> {
    // Envía un lote de eventos JSONL al master para que los reenvíe al worker.
    let events = read_events(file)?;
    if events.is_empty() {
        return Err(anyhow!("No hay eventos para enviar"));
    }
    let req = IngestRequest {
        topology_id: id,
        events,
    };
    client
        .post(format!("{}/api/v1/ingest", master_url))
        .json(&req)
        .send()
        .await?
        .error_for_status()?;
    println!("Eventos enviados");
    Ok(())
}

fn read_events(file: Option<PathBuf>) -> anyhow::Result<Vec<EventPayload>> {
    let reader: Box<dyn BufRead> = match file {
        Some(path) => Box::new(BufReader::new(
            File::open(&path).with_context(|| format!("No se puede abrir {:?}", path))?,
        )),
        None => Box::new(BufReader::new(io::stdin())),
    };
    let mut events = Vec::new();
    for line in reader.lines() {
        let line = line?;
        if line.trim().is_empty() {
            continue;
        }
        let mut value: serde_json::Value = serde_json::from_str(&line)?;
        let ts = extract_timestamp(&mut value)?;
        events.push(EventPayload {
            timestamp: ts,
            data: value,
        });
    }
    Ok(events)
}

fn extract_timestamp(value: &mut serde_json::Value) -> anyhow::Result<DateTime<Utc>> {
    let ts_value = value
        .get("timestamp")
        .or_else(|| value.get("ts"))
        .cloned()
        .ok_or_else(|| anyhow!("Cada evento debe tener el campo timestamp o ts"))?;
    let ts_str = ts_value
        .as_str()
        .ok_or_else(|| anyhow!("El timestamp debe ser string RFC3339"))?;
    let timestamp = DateTime::parse_from_rfc3339(ts_str)?.with_timezone(&Utc);
    Ok(timestamp)
}
