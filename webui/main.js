/**
 * K I D D Y B L A S T E R
 * WebUI
 *
 * @author Johannes Braun <johannes.braun@hannenz.de>
 * @version 2019-11-11
 */
const port = 4444;

var express = require('express');
var sqlite = require('sqlite3');
var CardReader = require('./card_reader.js');
var qwant = require('qwant-api');
var fileUpload = require('express-fileupload');
var db = new sqlite.Database('../../cards.sql');
var http = require('http');
var https = require('https');
var fs = require('fs');
var path = require('path');

var app = express();
var exphbs = require('express-handlebars');
var hbs = exphbs.create({
    defaultLayout: 'main',
    extname: '.hbs',
    helpers: {},
});

app.engine('.hbs', hbs.engine);
app.set('view engine', '.hbs');

app.use(express.static(path.join(__dirname, 'public')));
app.use(express.urlencoded());
app.use(express.json());
app.use(fileUpload({ debug: false }));

app.get('/', function(req, res) {
	res.redirect('/cards');
});

app.get('/cards', function(req, res) {
    db.all('SELECT * FROM cards', function(err, results) {
        if (err) {
            throw err;
        }
        res.render('cards/index', { cards: results });
    });
});

app.get('/cards/get/:id', function(req, res) {
	console.log(req.params.id);
	db.get('SELECT * FROM cards WHERE id=' + req.params.id, function(err, result) {
		if (err) throw err;


		// Get an image from QWANT image search
		qwant.search("images", {
			query: result.name,
			count: 1,
		}, function(err, data) {
			console.log(data.data.result.items[0].media_preview);
			result.image = data.data.result.items[0].media_preview;
			result.layout = false;
			res.render('partials/card', result);
		});
	});
});

app.get('/cards/edit/:id', function(req, res) {
	db.get('SELECT * FROM cards WHERE id=' + req.params.id, function(err, result) {
		res.render('cards/edit', result);
	});
});

app.post('/cards/edit/:id', function(req, res) {
	// get file upload, if any
	if (req.files && req.files.upload) {
		var upload = req.files.upload;
		var newFile = __dirname + '/public/images/' + upload.name;
		upload.mv(newFile, function(err) {
			// todo...
			if (err) {
				console.log('Moving image failed');
			}
			db.run('UPDATE cards SET name=?, uri=?, image=? WHERE id=?', [ req.body.name, req.body.uri, upload.name, req.body.id ], function(err) {
				if (err) throw err;
				res.redirect('/cards/edit/' + req.params.id);
			});
		});
	}
	else if (req.body.download) {
		// Download image from the internets
		console.log('downloading: ' + req.body.download);
		var url = new URL(req.body.download);
		var fileName = path.basename(url.pathname);
		var file = fs.createWriteStream("./public/images/" + fileName);

		var client = http;
		client = (url.protocol == 'https:') ? https : client;
		var dlreq = client.get(url, function(response) {
			response.pipe(file);
			file.on('finish', function() {
				file.close(function() {
					db.run('UPDATE cards SET name=?, uri=?, image=? WHERE id=?', [ req.body.name, req.body.uri, fileName, req.body.id ], function(err) {
						if (err) throw err;
						res.redirect('/cards/edit/' + req.params.id);
					});
				});
			});
		});
	}
	else {
		// Just update
		db.run('UPDATE cards SET name=?, uri=?, image=? WHERE id=?', [ req.body.name, req.body.uri, req.body.image, req.body.id ], function(err) {
			if (err) throw err;
			res.redirect('/cards/edit/' + req.params.id);
		});
	}
});

// app.get('/cards/read', function(req, res) {
//     var reader = new CardReader.CardReader();
//     reader.read();
// });

app.get('/cards/read', function(req, res) {
    res.render('cards/read');
});

app.get('/cards/delete/:id', function(req, res) {
	console.log('Deleting card #' + req.params.id);
	db.run('DELETE FROM cards WHERE id=?', [ req.params.id ], function(err) {
		if (err) throw err;
		res.redirect('/cards');
	});

});

app.get('/stream', function(req, res) {
    // req.socket.setTimeout(Infinity);

    res.writeHead(200, {
        'Content-Type': 'text/event-stream',
        'Cache-Control': 'no-cache',
        'Connection': 'keep-alive'
    });
    res.write('\n');

	var reader = new CardReader.CardReader();

	// Loop reading a card 
	(function doRead() {

		reader.read()
		.then(function(cardId) {

			// If a card has been detected, send its id to the client
			res.write("event: message\n");
			res.write('data: ' + JSON.stringify({
					"time": new Date().toString(),
					"cardId": cardId
			}) + "\n\n" );

			// and start over
			doRead();
		})
		.catch(function(err) {
			// We are on a rather old nodejs version, where `finally` is not yet
			// available, so we have to re-loop in both cases: resolve and
			// reject
			console.log(err);
			doRead();
		})
	})();
});

app.post('/imagesearch', function(req, res) {
	console.log(req.body);
	qwant.search("images", {
		query: req.body.query,
		count: 5,
	}, function(err, data) {
		res.json(data.data.result.items);
	});

});

app.listen(port);
console.log("Kiddyblaster WebUI has been started and is listening on port " + port);