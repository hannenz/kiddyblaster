/**
 * K I D D Y B L A S T E R
 * WebUI
 *
 * @author Johannes Braun <johannes.braun@hannenz.de>
 * @version 2019-11-11
 */
const port = 4444;
const audioDir = process.env.HOME; // + '/Musik';

var express = require('express');
var sqlite = require('sqlite3');
var CardReaderWriter = require('./card_reader_writer.js');
var qwant = require('qwant-api');
var fileUpload = require('express-fileupload');
var db = new sqlite.Database('/var/lib/kiddyblaster/cards.sql');
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

app.post('/cards/save', function(req, res) {
	// get file upload, if any
	if (req.files && req.files.upload) {
		var upload = req.files.upload;
		var newFile = __dirname + '/public/images/' + upload.name;
		upload.mv(newFile, function(err) {
			// todo...
			if (err) {
				console.log('Moving image failed');
			}

			if (req.body.id) {
				query = `UPDATE cards SET name='${req.body.name}', uri='${req.body.uri}', image='${upload.name}' WHERE id=${req.body.id}`;
			}
			else {
				query = `INSERT INTO cards SET name='${req.body.name}', uri='${req.body.uri}', image='${upload.name}'`; 
			}
			db.run(query, function(err) {
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
					if (req.body.id) {
						query = `UPDATE cards SET name='${req.body.name}', uri='${req.body.uri}', image='${fileName}' WHERE id=${req.body.id}`;
					}
					else {
						query = `INSERT INTO cards SET name='${req.body.name}', uri='${req.body.uri}', image='${fileName}'`; 
					}
					db.run(query, function(err) {
						if (err) throw err;
						res.redirect('/cards/edit/' + req.params.id);
					});
				});
			});
		});
	}
	else {
		// Just update
		if (req.body.id) {
			console.log(req.body.id);
			query = `UPDATE cards SET name='${req.body.name}', uri='${req.body.uri}', image='${req.body.image}' WHERE id=${req.body.id}`;
		}
		else {
			query = `INSERT INTO cards (name, uri, image)VALUES ('${req.body.name}', '${req.body.uri}', '${req.body.image}')`; 
		}
		console.log(query);
		db.run(query, function(err) {
			if (err) throw err;
			res.redirect('/cards/edit/' + req.params.id);
		});
	}
});


app.get('/cards/new', function(req, res) {
	res.render('cards/new');
});


app.get('/dir/get/:path?', function(req, res) {

	const path = (req.params.path) ? req.params.path : '';

	// TODO: Use file path builder if possible
	const dir = audioDir + '/' + path;

	fs.readdir(dir, { withFileTypes: true }, (err, items) => {
		if (err) {
			console.log('Error reading directory: ' + dir);
		}

		var results = [];
		if (items) {
			items.forEach(item => {
				results.push({
					name: item.name,
					isDir: item.isDirectory()
				});
			});
		}
		res.json(results);
	});
});


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

	var reader = new CardReaderWriter.CardReaderWriter();

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
