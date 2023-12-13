let socket = new WebSocket('ws://localhost:8080');

const saveButton = document.getElementById('save-still');
const view = document.getElementById('stream');
const apagarAlarma = document.getElementById('apagarAlarma');
const flashButton = document.getElementById('flashButton');
const downloadVideo = document.getElementById('downloadVideo');
const modeButton = document.getElementById('modeButton');

const data = { Movimiento: 1 };
let modo = 'vigilancia';
let flash = 0;

let toastContainer;
const generateToast = ({ message, backgroundColor, length = '3000ms', icon }) => {
	toastContainer.insertAdjacentHTML(
		'beforeend',
		`<p class="toast ${backgroundColor} text-2xl font-bold p-4" 
    style="
    animation-duration: ${length}">
	<i class="${icon} mr-2"></i>
    ${message}
  </p>`
	);
	const toast = toastContainer.lastElementChild;
	toast.addEventListener('animationend', () => toast.remove());
};

const initToast = () => {
	document.body.insertAdjacentHTML(
		'afterbegin',
		`<div class="toast-container fixed top-4 right-6 grid justify-end gap-6 z-10"></div>
	<style>
  
  .toast {
	animation: toastIt 3000ms cubic-bezier(0.785, 0.135, 0.15, 0.86) forwards;
  }
  
  @keyframes toastIt {
	0%,
	100% {
	  transform: translateY(-150%);
	  opacity: 0;
	}
	10%,
	90% {
	  transform: translateY(0);
	  opacity: 1;
	}
  }
	</style>
	`
	);
	toastContainer = document.querySelector('.toast-container');
};

initToast();

saveButton.onclick = () => {
	let canvas = document.createElement('canvas');
	canvas.width = view.width;
	canvas.height = view.height;
	canvas.style.display = 'none';
	document.body.appendChild(canvas);
	let context = canvas.getContext('2d');
	context.drawImage(view, 0, 0);
	generateToast({
		message: 'Descargando imagen...',
		backgroundColor: 'bg-blue-500',
		length: '5000ms',
		icon: 'fas fa-file-download',
	});

	try {
		let dataURL = canvas.toDataURL('image/jpeg');
		saveButton.href = dataURL;
		let name = new Date();
		console.log(name);
		saveButton.download = name.getDate() + '-' + ('0' + (name.getMonth() + 1)).slice(-2) + '-' + name.getFullYear() + '--' + ('0' + name.getHours()).slice(-2) + '-' + ('0' + name.getMinutes()).slice(-2) + '-' + ('0' + name.getSeconds()).slice(-2) + '.jpg';
	} catch (e) {
		console.error(e);
	}
	canvas.parentNode.removeChild(canvas);
};

apagarAlarma.onclick = () => {
	socket.send('{"apagarAlarma": 1}');
	generateToast({
		message: 'La alarma fue apagada',
		backgroundColor: 'bg-blue-500',
		length: '5000ms',
		icon: 'fas fa-info-circle',
	});
};

flashButton.onclick = () => {
	if (!flash) {
		socket.send('{"flash": 1}');
		flash = 1;
		flashButton.innerHTML = 'Apagar Flash <i class="far fa-sun ml-3"></i>';
	} else {
		socket.send('{"flash": 0}');
		flash = 0;
		flashButton.innerHTML = 'Encender Flash <i class="fas fa-sun ml-3"></i>';
	}
};

downloadVideo.onclick = () => {
	socket.send('{"Video": false}');
	generateToast({
		message: 'Descargando video...',
		backgroundColor: 'bg-blue-500',
		length: '5000ms',
		icon: 'fas fa-file-download',
	});
	let endTime = Date.now() + 10000; // Fin del intervalo
	let counter = 0;
	const interval = setInterval(() => {
		let canvas = document.createElement('canvas');
		canvas.width = view.width;
		canvas.height = view.height;
		document.body.appendChild(canvas);
		let context = canvas.getContext('2d');
		context.drawImage(view, 0, 0);

		try {
			counter = counter + 1;
			let imageName = 'captura' + counter + '.jpg';
			console.log(imageName);
			socket.binaryType = 'arraybuffer';
			// envia la imagen como datos binarios
			socket.send(
				canvas.toBlob(function (blob) {
					socket.send(blob), { binary: true };
					socket.send(imageName); // envia el nombre de la imagen
				}, 'image/jpeg')
			);
		} catch (e) {
			console.error(e);
		}
		canvas.parentNode.removeChild(canvas);
		if (Date.now() >= endTime) {
			socket.send('{"Video": true}');
			clearInterval(interval); // Cuando el intervalo finaliza detiene la captura
		}
	}, 39);
};

modeButton.onclick = () => {
	if (modo == 'vigilancia') {
		modo = 'nocturno';
		modeButton.innerHTML = 'Modo nocturno <i class="fas fa-moon ml-3"></i>';
	} else {
		modo = 'vigilancia';
		modeButton.innerHTML = 'Modo vigilancia <i class="fas fa-eye ml-3"></i>';
	}

	socket.send(`{"modo": "${modo}"}`);
	console.log(`modo ${modo}`);
};

socket.onmessage = function (event) {
	let data = JSON.parse(event.data);

	//itera por todas las claves del json
	Object.keys(data).forEach((key) => {
		if (key.startsWith('modo')) {
			if (data[key] == 'vigilancia') {
				modo = 'vigilancia';
				modeButton.innerHTML = 'Modo vigilancia';
			} else if (data[key] == 'nocturno') {
				modo = 'nocturno';
				modeButton.innerHTML = 'Modo nocturno';
			}
		}
		if (modo == 'vigilancia') {
			if (key.startsWith('sensorPIR')) {
				if (data[key] == 1) {
					saveButton.click();
				}
			}
		}
		if (modo == 'nocturno') {
			if (key.startsWith('sensorPIR')) {
				if (data[key] == 1) {
					generateToast({
						message: 'Alarma activada',
						backgroundColor: 'bg-red-500',
						length: '10000ms',
						icon: 'fas fa-exclamation-triangle',
					});
					downloadVideo.click();
					console.log('alarma activada');
				}
			}
		}
	});
};
