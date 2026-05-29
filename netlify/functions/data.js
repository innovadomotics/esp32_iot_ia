import { getStore } from '@netlify/blobs';

export default async (req, context) => {
  const sensorStore = getStore('sensor-data');
  const commandStore = getStore('commands');
  const SENSOR_KEY = 'latest';
  const COMMAND_KEY = 'current';

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

    const { temperatura, humedad } = body;
    if (typeof temperatura !== 'number' || typeof humedad !== 'number') {
      return new Response(
        JSON.stringify({ error: 'Se requieren campos numéricos: temperatura, humedad' }),
        { status: 400, headers: { 'Content-Type': 'application/json', ...cors } }
      );
    }

    const record = {
      temperatura,
      humedad,
      timestamp: new Date().toISOString(),
    };

    await sensorStore.setJSON(SENSOR_KEY, record);

    // Piggyback: adjuntamos los comandos actuales para el ESP32
    const comandos = (await commandStore.get(COMMAND_KEY, { type: 'json' })) || {
      switch: false,
      texto: '',
    };

    return new Response(JSON.stringify({ ok: true, ...record, comandos }), {
      status: 200,
      headers: { 'Content-Type': 'application/json', ...cors },
    });
  }

  if (req.method === 'GET') {
    const data = (await sensorStore.get(SENSOR_KEY, { type: 'json' })) || {};
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
  path: '/.netlify/functions/data',
};
