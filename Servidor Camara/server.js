// ejecutar npx tailwindcss -i ./public/input.css -o ./public/output.css --watch

const express = require('express');
const app = express();
const httpServer = require('http').createServer(app);
const ip = require('ip');
const PORT = 80;
const fs = require('fs');
const server = require('ws').Server;
const ffmpeg = require('fluent-ffmpeg');
const path = require('path');

httpServer.listen(PORT, () => {
	console.log(`Servidor web activo http://${ip.address()}:${PORT}`);
});

app.use(express.static(__dirname + '/public'));
app.use(express.urlencoded({ extended: true }));

app.get('/home', (req, res) => {
	res.sendFile(__dirname + '/public/home.html');
});

app.get('/login', (req, res) => {
	res.sendFile(__dirname + '/public/login.html');
});

app.post('/login', async (req, res) => {
	try {
		const usuarioIngresado = req.body.usuario;
		const passwordIngresado = req.body.password;
		const usuarios = JSON.parse(await fs.promises.readFile('./usuarios.json'));

		const usuarioEncontrado = usuarios.find((usuario) => usuario.usuario == usuarioIngresado && usuario.password == passwordIngresado);
		if (usuarioEncontrado) {
			res.redirect('/home');
		} else {
			//toast
			res.status(401).redirect('/faillogin');
		}
	} catch (err) {
		console.log(err);
		res.status(500).json({ error: err });
	}
});

app.get('/faillogin', (req, res) => {
	res.sendFile(__dirname + '/public/faillogin.html');
});

app.get('*', (req, res) => {
	res.redirect('/login');
});

let directoryPath = '';
let folderName = '';

let s = new server({ port: 8080 });
s.on('connection', function (ws) {
	console.log('Cliente conectado');

	let imageName = null;
	let imageData = null;

	ws.on('message', async (message) => {
		string = message.toString();

		if (string.startsWith('{')) {
			// Transmite el mensaje a todos los clientes menos al transmisor

			s.clients.forEach(function each(client) {
				if (client !== ws) {
					client.send(string);
				}
			});
			// itera las claves JSON

			let data = JSON.parse(string);
			console.log('Json recibido: ' + string);
			Object.keys(data).forEach((key) => {
				if (key.startsWith('Video')) {
					if (data[key] == false) {
						//crea carpeta con el timestamp actual

						let date = new Date();
						folderName = date.getDate() + '-' + ('0' + (date.getMonth() + 1)).slice(-2) + '-' + date.getFullYear() + '--' + ('0' + date.getHours()).slice(-2) + '-' + ('0' + date.getMinutes()).slice(-2) + '-' + ('0' + date.getSeconds()).slice(-2);
						directoryPath = `./images/${folderName}`;
						if (!fs.existsSync(directoryPath)) {
							fs.mkdirSync(directoryPath);
							return directoryPath;
						}
					} else if (data[key] == true) {
						/// ffmpeg con fs

						const outputFilePath = path.join(__dirname + '/images/', `${folderName}.mp4`); // Ruta de salida
						ffmpeg()
							.input(`${directoryPath}/captura%d.jpg`) // Patron de imagenes de entrada
							.output(outputFilePath)
							.withVideoCodec('libx264')
							.withFps(60)

							.videoFilters('scale=626:470') // Escalado del video
							.on('end', () => {
								// Cuando termina, borra la carpeta donde estan almacenadas las imagenes
								fs.rm(directoryPath, { recursive: true }, (err) => {
									if (err) {
										console.log('cant remove');
										console.error(err);
									} else {
										console.log(`El directorio fue eliminado exitosamente ${directoryPath}`);
									}
								});
							})
							.on('error', (err, stdout, stderr) => {
								if (err) {
									console.log(err.message);
									console.log('stdout:\n' + stdout);
									console.log('stderr:\n' + stderr);
								}
							})
							.run();
						console.log('video guardado');
					}
				}
			});
		} else if (string.includes('.jpg')) {
			imageName = string;
		} else {
			imageData = Buffer.from(message);
			if (imageName && imageData) {
				// guardar datos de imagen a archivo
				await fs.promises.writeFile(`${directoryPath}/` + imageName, imageData, (err) => {
					if (err) {
						console.error(err);
					} else {
						console.log('Image saved to ' + imageName);
						imageName = null;
						imageData = null;
					}
				});
			}
		}
	});

	ws.on('close', function () {
		console.log('Cliente desconectado');
	});
});
