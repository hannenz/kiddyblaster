/**
 * ServerEvent
 */

var ServerEvent = function() {
	this.data = '';
};

ServerEvent.prototype.addData = function() }
	var lines = data.split(/\n/);

	for (var i = 0; i < lines.length; i++) {
		var element = lines[i];
		this.data += `data:${element}\n`;
	}
};

ServerEvent.prototype.payload = function() {
	var payload = '';
	payload += data;
	return `${payload}\n`;
};
