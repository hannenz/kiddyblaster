var FileSelect = function() {

	var uriInput = document.querySelector('[name=uri]');
	var uriGhostInput = document.querySelector('[name=uri-ghost]');
	var dlg = document.getElementById('file-select-dialog');
	var fileList = document.getElementById('file-list');

	uriInput.addEventListener('focus', function() {
		uriGhostInput.value = uriInput.value;
		dlg.hidden = false;
		document.addEventListener('keyup', onKeyUp);
		loadDir(this.value);
	});


	function onKeyUp(ev) {
		if (ev.keyCode == 27) {
			closeDialog();
		}
	}


	function closeDialog() {
		dlg.hidden = true;
		document.removeEventListener('keyup', onKeyUp);
	}


	function loadDir(path) {
		var xhr = new XMLHttpRequest();
		xhr.open('GET', '/dir/get/' + encodeURIComponent(path));
		xhr.onload = function() {
			fileList.innerHTML = '';
			var result = JSON.parse(this.responseText);

			var closeBtn = document.getElementById('file-select-dialog-close-btn');
			closeBtn.addEventListener('click', function() {
				closeDialog();
			});
			var applyBtn = document.getElementById('file-select-dialog-apply-btn');
			applyBtn.addEventListener('click', function() {
				uriInput.value = uriGhostInput.value;
				closeDialog();
			});

			if (uriGhostInput.value.length > 0) {
				var parentListItem = document.createElement('li');
				var parentLink = document.createElement('a');
				parentLink.innerText = '..';
				parentLink.setAttribute('href', 'javascript:;');
				parentLink.className = 'go-up';
				parentLink.addEventListener('click', onParentLinkClicked);
				parentListItem.appendChild(parentLink);
				fileList.appendChild(parentListItem);
			}

			result.forEach(function(item) {
				if (item.name.indexOf('.') != 0) {

					var listItem = document.createElement('li');
					var link = document.createElement('a');
					link.innerText = item.name;
					link.className = item.isDir ? 'dir' : 'file';
					if (item.isDir) {
						link.setAttribute('href', 'javascript:;');
						link.addEventListener('click', onFileSelectFileClicked);
					}
					listItem.appendChild(link);
					fileList.appendChild(listItem);
				}
			});
		};
		xhr.send();
	}

	function onFileSelectFileClicked() {
		if (uriGhostInput.value.length == 0) {
			uriGhostInput.value = this.innerText;
		}
		else {
			uriGhostInput.value = uriGhostInput.value + '/' + this.innerText;
		}
		loadDir(uriGhostInput.value);
	}

	function onParentLinkClicked() {
		var matches = uriGhostInput.value.match(/(.*)\//);
		uriGhostInput.value = (matches != null)  ? matches[1] : '';
		loadDir(uriGhostInput.value);
	}

};
