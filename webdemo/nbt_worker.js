importScripts('nbt_viewer.js');

const moduleReady = createNbtViewer({
  mainScriptUrlOrBlob: new URL('nbt_viewer.js', self.location.href).href
});

self.onmessage = async ({ data }) => {
  let module;
  let pointer = 0;
  try {
    module = await moduleReady;
    const bytes = new Uint8Array(data.buffer);
    if (!bytes.length) throw new Error('文件为空。');
    pointer = module._malloc(bytes.length);
    if (!pointer) throw new Error('没有足够的内存读取文件。');
    module.HEAPU8.set(bytes, pointer);
    const output = module._nbt_to_string(pointer, bytes.length);
    if (!output) throw new Error(module.UTF8ToString(module._nbt_last_error()));
    const text = new TextDecoder().decode(module.HEAPU8.slice(output, output + module._nbt_result_size()));
    self.postMessage({ text });
  } catch (error) {
    self.postMessage({ error: error.message || String(error) });
  } finally {
    if (pointer) module._free(pointer);
  }
};
