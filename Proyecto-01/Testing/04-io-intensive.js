// Pruebas IO Intensivas - Archivos >= 50MB
import http from 'k6/http';
import { check } from 'k6';

export const options = {
  scenarios: {
    sortfile: {
      executor: 'constant-vus',
      exec: 'testSortFile',
      vus: 3,
      duration: '60s',
    },
    compress: {
      executor: 'constant-vus',
      exec: 'testCompress',
      vus: 3,
      duration: '60s',
      startTime: '0s',
    },
    hashfile: {
      executor: 'constant-vus',
      exec: 'testHashFile',
      vus: 3,
      duration: '60s',
      startTime: '0s',
    },
  },
  thresholds: {
    'http_req_duration{operation:sortfile}': ['p(95)<120000'],
    'http_req_duration{operation:compress}': ['p(95)<60000'],
    'http_req_duration{operation:hashfile}': ['p(95)<30000'],
    'checks': ['rate>0.85'],
  },
};

const BASE = 'http://localhost:8080';

export function testSortFile() {
  const res = http.get(`${BASE}/sortfile?name=data-sort-50mb.txt&algo=merge`, {
    tags: { operation: 'sortfile' },
    timeout: '180s',
  });
  
  check(res, {
    'sortfile status ok': r => r.status === 200 || r.status === 408,
  });
}

export function testCompress() {
  const res = http.get(`${BASE}/compress?name=data-sort-50mb.txt&codec=gzip&impl=lib`, {
    tags: { operation: 'compress' },
    timeout: '90s',
  });
  
  check(res, {
    'compress status ok': r => r.status === 200 || r.status === 408,
  });
}

export function testHashFile() {
  const res = http.get(`${BASE}/hashfile?name=data-sort-50mb.txt&algo=sha256`, {
    tags: { operation: 'hashfile' },
    timeout: '60s',
  });
  
  check(res, {
    'hashfile status ok': r => r.status === 200,
  });
}

export function handleSummary(data) {
  const sortP50 = data.metrics['http_req_duration{operation:sortfile}'].values['p(50)'];
  const sortP95 = data.metrics['http_req_duration{operation:sortfile}'].values['p(95)'];
  const sortP99 = data.metrics['http_req_duration{operation:sortfile}'].values['p(99)'];
  
  const compP50 = data.metrics['http_req_duration{operation:compress}'].values['p(50)'];
  const compP95 = data.metrics['http_req_duration{operation:compress}'].values['p(95)'];
  const compP99 = data.metrics['http_req_duration{operation:compress}'].values['p(99)'];
  
  const hashP50 = data.metrics['http_req_duration{operation:hashfile}'].values['p(50)'];
  const hashP95 = data.metrics['http_req_duration{operation:hashfile}'].values['p(95)'];
  const hashP99 = data.metrics['http_req_duration{operation:hashfile}'].values['p(99)'];
  
  console.log('\n========================================');
  console.log('  IO INTENSIVE TEST RESULTS (50MB+)');
  console.log('========================================');
  console.log('SORTFILE (merge sort):');
  console.log(`  p50: ${(sortP50/1000).toFixed(2)}s`);
  console.log(`  p95: ${(sortP95/1000).toFixed(2)}s`);
  console.log(`  p99: ${(sortP99/1000).toFixed(2)}s`);
  console.log('\nCOMPRESS (gzip):');
  console.log(`  p50: ${(compP50/1000).toFixed(2)}s`);
  console.log(`  p95: ${(compP95/1000).toFixed(2)}s`);
  console.log(`  p99: ${(compP99/1000).toFixed(2)}s`);
  console.log('\nHASHFILE (sha256):');
  console.log(`  p50: ${(hashP50/1000).toFixed(2)}s`);
  console.log(`  p95: ${(hashP95/1000).toFixed(2)}s`);
  console.log(`  p99: ${(hashP99/1000).toFixed(2)}s`);
  console.log('========================================\n');
  
  return { 'stdout': '' };
}