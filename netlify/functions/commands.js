import { getStore } from '@netlify/blobs';

export default async (req, context) => {
  const store = getStore('commands');
  const KEY = 'current';

  const cors = {
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type',
  };

  if (req.method === 'OPTIONS') {
    return new Response(null, { status: 204, headers: cors });
  }

  if (req.method === 'POST') {
    let body;
    try {
      body = await req.json();
    } catch (e) {
      return new Response(JSON.stringify({ error: 'JSON inválido' }), {
        status: 400,
        headers: { 'Content-Type': 'application/json', ...cors },
      });
    }

    const record = {
      switch: !!body.switch,
      texto: typeof body.texto === 'string' ? body.texto : '',
      timestamp: new Date().toISOString(),
    };

    await store.setJSON(KEY, record);

    return new Response(JSON.stringify({ ok: true, ...record }), {
      status: 200,
      headers: { 'Content-Type': 'application/json', ...cors },
    });
  }

  if (req.method === 'GET') {
    const data = (await store.get(KEY, { type: 'json' })) || {
      switch: false,
      texto: '',
    };
    return new Response(JSON.stringify(data), {
      status: 200,
      headers: { 'Content-Type': 'application/json', ...cors },
    });
  }

  return new Response(JSON.stringify({ error: 'Método no permitido' }), {
    status: 405,
    headers: { 'Content-Type': 'application/json', ...cors },
  });
};

export const config = {
  path: '/.netlify/functions/commands',
};
