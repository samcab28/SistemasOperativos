// Pruebas CPU Intensivas - Operaciones que toman varios segundos
import http from 'k6/http';
import { check } from 'k6';

export const options = {
  scenarios: {
    pi: {
      executor: 'constant-vus',
      exec: 'testPi',
      vus: 4,
      duration: '60s',
    },
    matrixmul: {
      executor: 'constant-vus',
      exec: 'testMatrix',
      vus: 4,
      duration: '60s',
      startTime: '0s',
    },
    mandelbrot: {
      executor: 'constant-vus',
      exec: 'testMandelbrot',
      vus: 3,
      duration: '60s',
      startTime: '0s',
    },
  },
  thresholds: {
    'http_req_duration{operation:pi}': ['p(95)<60000'],
    'http_req_duration{operation:matrix}': ['p(95)<60000'],
    'http_req_duration{operation:mandelbrot}': ['p(95)<60000'],
    'checks': ['rate>0.85'],
  },
};

const BASE = 'http://localhost:8080';

export function testPi() {
  const res = http.get(`${BASE}/pi?digits=2000&algo=spigot`, {
    tags: { operation: 'pi' },
    timeout: '90s',
  });
  
  check(res, {
    'pi status ok': r => r.status === 200 || r.status === 408,
  });
}

export function testMatrix() {
  const res = http.get(`${BASE}/matrixmul?size=512&seed=42`, {
    tags: { operation: 'matrix' },
    timeout: '90s',
  });
  
  check(res, {
    'matrix status ok': r => r.status === 200 || r.status === 408,
  });
}

export function testMandelbrot() {
  const res = http.get(`${BASE}/mandelbrot?width=2048&height=1536&max_iter=2000`, {
    tags: { operation: 'mandelbrot' },
    timeout: '90s',
  });
  
  check(res, {
    'mandelbrot status ok': r => r.status === 200 || r.status === 408,
  });
}

export function handleSummary(data) {
  const piP50 = data.metrics['http_req_duration{operation:pi}'].values['p(50)'];
  const piP95 = data.metrics['http_req_duration{operation:pi}'].values['p(95)'];
  const piP99 = data.metrics['http_req_duration{operation:pi}'].values['p(99)'];
  
  const matrixP50 = data.metrics['http_req_duration{operation:matrix}'].values['p(50)'];
  const matrixP95 = data.metrics['http_req_duration{operation:matrix}'].values['p(95)'];
  const matrixP99 = data.metrics['http_req_duration{operation:matrix}'].values['p(99)'];
  
  const mandelP50 = data.metrics['http_req_duration{operation:mandelbrot}'].values['p(50)'];
  const mandelP95 = data.metrics['http_req_duration{operation:mandelbrot}'].values['p(95)'];
  const mandelP99 = data.metrics['http_req_duration{operation:mandelbrot}'].values['p(99)'];
  
  console.log('\n========================================');
  console.log('  CPU INTENSIVE TEST RESULTS');
  console.log('========================================');
  console.log('PI CALCULATION (2000 digits):');
  console.log(`  p50: ${(piP50/1000).toFixed(2)}s`);
  console.log(`  p95: ${(piP95/1000).toFixed(2)}s`);
  console.log(`  p99: ${(piP99/1000).toFixed(2)}s`);
  console.log('\nMATRIX MULTIPLY (512x512):');
  console.log(`  p50: ${(matrixP50/1000).toFixed(2)}s`);
  console.log(`  p95: ${(matrixP95/1000).toFixed(2)}s`);
  console.log(`  p99: ${(matrixP99/1000).toFixed(2)}s`);
  console.log('\nMANDELBROT (2048x1536, 2000 iter):');
  console.log(`  p50: ${(mandelP50/1000).toFixed(2)}s`);
  console.log(`  p95: ${(mandelP95/1000).toFixed(2)}s`);
  console.log(`  p99: ${(mandelP99/1000).toFixed(2)}s`);
  console.log('========================================\n');
  
  return { 'stdout': '' };
}