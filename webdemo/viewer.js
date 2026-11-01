const fileInput = document.getElementById('file');
const status = document.getElementById('status');
const result = document.getElementById('result');

if (!crossOriginIsolated) {
  fileInput.disabled = true;
  status.textContent = '请先运行 python webdemo/serve.py，再通过 http://127.0.0.1:8000 打开页面。';
}

fileInput.addEventListener('change', async () => {
  const file = fileInput.files[0];
  if (!file) return;
  fileInput.disabled = true;
  result.textContent = '';
  status.textContent = `正在解析 ${file.name}（${file.size} 字节）…`;
  let worker;
  let timeout;
  try {
    const buffer = await file.arrayBuffer();
    worker = new Worker('nbt_worker.js');
    const text = await new Promise((resolve, reject) => {
      timeout = setTimeout(() => reject(new Error('解析超过 30 秒，请尝试较小的文件。')), 30000);
      worker.onmessage = ({ data }) => data.error ? reject(new Error(data.error)) : resolve(data.text);
      worker.onerror = (event) => {
        event.preventDefault();
        reject(new Error(event.message || '无法加载或运行 NBT 解析器。'));
      };
      worker.postMessage({ buffer }, [buffer]);
    });
    result.textContent = text;
    status.textContent = `${file.name}：解析完成（${file.size} 字节）。`;
  } catch (error) {
    status.textContent = `解析失败：${error.message}`;
  } finally {
    clearTimeout(timeout);
    worker?.terminate();
    fileInput.disabled = false;
    fileInput.value = '';
  }
});
