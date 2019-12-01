const port = 3001;

var express = require('express');
var sqlite = require('sqlite3');
var CardReader = require('./card_reader.js');
var qwant = require('qwant-api');

var db = new sqlite.Database('/home/pi/cards.sql');

var app = express();
var exphbs = require('express-handlebars');
var hbs = exphbs.create({
    defaultLayout: 'main',
    extname: '.hbs',
    helpers: {}
});

app.engine('.hbs', hbs.engine);
app.set('view engine', '.hbs');

app.use(express.static(__dirname + '/dist'));
app.use(express.urlencoded());
app.use(express.json());

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
			res.render('cards/card', result);
		});
	});
});

// app.get('/cards/read', function(req, res) {
//     var reader = new CardReader.CardReader();
//     reader.read();
// });

app.get('/cards/read', function(req, res) {
    res.render('cards/read');
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

app.listen(port);
console.log("Kiddyblaster WebUI has been started and is listening on port " + port);
