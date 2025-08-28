const express = require('express');
const path = require('path');

const app = express();
const port = 8080;

// 设置跨源头信息
app.use((req, res, next) => {
  res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
  res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
  next();
});

// 提供静态文件服务
app.use(express.static(path.join(__dirname, 'dist')));
app.use(express.static(__dirname));

// 启动服务器
app.listen(port, () => {
  console.log(`Server running at http://localhost:${port}`);
});
