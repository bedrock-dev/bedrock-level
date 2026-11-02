const fileInput = document.getElementById('file');
const status = document.getElementById('status');
const result = document.getElementById('result');

if (!crossOriginIsolated) {
  fileInput.disabled = true;
  status.textContent = 'Run python webdemo/serve.py, then open http://127.0.0.1:8000.';
}

fileInput.addEventListener('change', async () => {
  const file = fileInput.files[0];
  if (!file) return;
  fileInput.disabled = true;
  result.textContent = '';
  status.textContent = `Parsing ${file.name} (${file.size} bytes)...`;
  let worker;
  let timeout;
  try {
    const buffer = await file.arrayBuffer();
    worker = new Worker('nbt_worker.js');
    const text = await new Promise((resolve, reject) => {
      timeout = setTimeout(() => reject(new Error('Parsing took more than 30 seconds. Try a smaller file.')), 30000);
      worker.onmessage = ({ data }) => data.error ? reject(new Error(data.error)) : resolve(data.text);
      worker.onerror = (event) => {
        event.preventDefault();
        reject(new Error(event.message || 'Could not load or run the NBT parser.'));
      };
      worker.postMessage({ buffer }, [buffer]);
    });
    result.textContent = text;
    status.textContent = `${file.name}: parsing complete (${file.size} bytes).`;
  } catch (error) {
    status.textContent = `Parsing failed: ${error.message}`;
  } finally {
    clearTimeout(timeout);
    worker?.terminate();
    fileInput.disabled = false;
    fileInput.value = '';
  }
});
