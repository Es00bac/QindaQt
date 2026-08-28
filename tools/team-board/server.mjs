import { createServer } from 'node:http';
import { readFile, readdir, realpath, stat } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { readBoard } from './board.mjs';

const moduleDirectory = path.dirname(fileURLToPath(import.meta.url));
const defaultTeamRoot = path.resolve(moduleDirectory, '../../ops/team');

export function parseServerArguments(argv = process.argv.slice(2)) {
  const parsed = { port: 4179, teamRoot: defaultTeamRoot };
  for (let index = 0; index < argv.length; index += 1) {
    if (argv[index] === '--port') {
      const port = Number(argv[index + 1]);
      if (!Number.isInteger(port) || port < 1 || port > 65_535) throw new RangeError('port must be 1..65535');
      parsed.port = port;
      index += 1;
    } else if (argv[index] === '--team-root') {
      if (!argv[index + 1]) throw new TypeError('--team-root requires a directory path');
      parsed.teamRoot = path.resolve(argv[index + 1]);
      index += 1;
    } else {
      throw new TypeError(`unknown argument: ${argv[index]}`);
    }
  }
  return Object.freeze(parsed);
}

export function createTeamBoardServer({ teamRoot = defaultTeamRoot } = {}) {
  const messagesRoot = path.resolve(teamRoot, 'messages');
  const confinedPath = async (...parts) => {
    const root = await realpath(messagesRoot);
    const target = await realpath(path.join(messagesRoot, ...parts));
    if (target !== root && !target.startsWith(`${root}${path.sep}`)) throw new Error('Message path escapes configured messages tree');
    return target;
  };
  const readMessages = async () => {
    const root = await realpath(messagesRoot); const entries = await readdir(root, { withFileTypes: true }); const threads = [];
    for (const entry of entries.sort((a, b) => a.name.localeCompare(b.name))) {
      if (!/^_?[a-z0-9][a-z0-9-]*$/.test(entry.name) || !entry.isDirectory()) continue;
      let threadPath; try { threadPath = await confinedPath(entry.name); } catch { continue; }
      const replies = [];
      for (const reply of (await readdir(threadPath, { withFileTypes: true })).sort((a, b) => a.name.localeCompare(b.name))) {
        if (!reply.isFile() || !/^\d+-[^/]+\.md(?:\.done)?$/.test(reply.name)) continue;
        try { const target = await confinedPath(entry.name, reply.name); const info = await stat(target); const closed = reply.name.endsWith('.md.done'); replies.push({ id: reply.name, closed, label: `${reply.name.replace(/^\d+-/, '').replace(/\.md(?:\.done)?$/, '').replaceAll('-', ' ')}${closed ? ' (closed)' : ''}`, bytes: info.size }); } catch { /* rejected escape */ }
      }
      const closed = entry.name.startsWith('_');
      threads.push({ id: entry.name, closed, label: `${entry.name.replace(/^_/, '').replaceAll('-', ' ')}${closed ? ' (closed)' : ''}`, replies });
    }
    return threads;
  };
  return createServer(async (request, response) => {
    try {
      if (request.method !== 'GET') {
        response.writeHead(405, { Allow: 'GET' });
        response.end();
        return;
      }
      if (request.url === '/api/board') {
        const board = await readBoard(teamRoot);
        const body = `${JSON.stringify(board)}\n`;
        response.writeHead(200, {
          'Cache-Control': 'no-store',
          'Content-Type': 'application/json; charset=utf-8',
          'Content-Length': Buffer.byteLength(body),
        });
        response.end(body);
        return;
      }
      if (request.url === '/api/messages') {
        const body = `${JSON.stringify(await readMessages())}\n`;
        response.writeHead(200, { 'Cache-Control': 'no-store', 'Content-Type': 'application/json; charset=utf-8', 'Content-Length': Buffer.byteLength(body) }); response.end(body); return;
      }
      if (request.url.startsWith('/api/messages/')) {
        const parts = request.url.slice('/api/messages/'.length).split('/').map((part) => decodeURIComponent(part));
        if (parts.length !== 2 || !/^_?[a-z0-9][a-z0-9-]*$/.test(parts[0]) || !/^\d+-[^/]+\.md(?:\.done)?$/.test(parts[1])) { response.writeHead(400); response.end('Bad message path'); return; }
        const target = await confinedPath(...parts); const body = await readFile(target, 'utf8');
        response.writeHead(200, { 'Cache-Control': 'no-store', 'Content-Type': 'text/plain; charset=utf-8', 'Content-Length': Buffer.byteLength(body) }); response.end(body); return;
      }
      if (request.url === '/' || request.url === '/index.html') {
        const body = await readFile(path.join(moduleDirectory, 'public/index.html'));
        response.writeHead(200, { 'Cache-Control': 'no-store', 'Content-Type': 'text/html; charset=utf-8' });
        response.end(body);
        return;
      }
      if (request.url === '/markdown.js') {
        const body = await readFile(path.join(moduleDirectory, 'public/markdown.js'));
        response.writeHead(200, { 'Cache-Control': 'no-store', 'Content-Type': 'text/javascript; charset=utf-8' }); response.end(body); return;
      }
      response.writeHead(404);
      response.end();
    } catch (error) {
      response.writeHead(500, { 'Cache-Control': 'no-store', 'Content-Type': 'application/json; charset=utf-8' });
      response.end(`${JSON.stringify({ error: error instanceof Error ? error.message : String(error) })}\n`);
    }
  });
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const { port, teamRoot } = parseServerArguments();
  const server = createTeamBoardServer({ teamRoot });
  server.listen(port, '127.0.0.1', () => {
    process.stdout.write(`Team Board listening on http://127.0.0.1:${port} (team root ${teamRoot})\n`);
  });
}
