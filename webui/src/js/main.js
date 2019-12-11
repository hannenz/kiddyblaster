var KiddyBlaster = function() {

	var self = this;

	this.init = function() {
		console.log("init");
		self.initButtons();
		var cardEdit = new CardEdit();
	};

	this.initButtons = function() {

		console.log("initButtons");
		var deleteButtons = document.querySelectorAll('.delete');
		deleteButtons.forEach(function(btn) {
			btn.addEventListener('click', function(ev) {
				ev.preventDefault();

				var id = btn.dataset.id;
				if (!confirm("Soll die Karte mit der ID " + id + " wirklich gelöscht werden?")) {
					return;
				}

				var req = new XMLHttpRequest();
				req.open('GET', '/cards/delete/' + id);
				req.onload = function(ev) {
					if (req.status < 200 || req.status >= 400) {
						return;
					}
					var frag = new DocumentFragment();
					var temp = document.createElement('div');
					temp.innerHTML = req.responseText;
					while(temp.firstChild) {
						frag.appendChild(temp.firstChild);
					}

					var newList = frag.querySelector('.cards');
					console.log(newList);
					var list = document.querySelector('.cards');
					console.log(list);

					var parent = list.parentNode;
					parent.insertBefore(newList, list);
					parent.removeChild(list);
				};

				req.send();
			});
		});
	};
};

document.addEventListener('DOMContentLoaded', function() {
	console.log("DOMContentLoaded");
	var kiddyBlaster = new KiddyBlaster();
	kiddyBlaster.init();
});


