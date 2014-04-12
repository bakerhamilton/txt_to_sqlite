/*
 * qserv.js
 *
 * 1/28/13
 * Baker Hamilton, M.D.
 *
 * sets up an http server to generate random questions from a sqlite3 database
 * see accompanying parse2db.c for more information, or go to
 * http://er2pc.blogspot.com/2013/01/building-quiz-engine-part-2.html
 */

var sqlite3 = require('sqlite3').verbose();
var db = new sqlite3.Database("./quiz.db");
var http = require('http');

http.createServer(function (req, res) {
	var random = Math.floor(Math.random()*450)+1;
	
	db.get("select * from quiz where qnumber=" + random, function(err, row) {
		res.writeHead(200, {'Content-Type': 'text/html'});
		res.write('<html>');
		res.write('<style>.blank-page {margin-top: 6in;}</style>');
		res.write(row.question + '<p><p>');

		if (row.image) {
			var b64image = new Buffer(row.image).toString('base64');
			res.write('<img src="data:image/png;base64,' + b64image + '"><p>');
		}

		res.write(row.choiceA + '<p>' + row.choiceB + '<p>' + row.choiceC + '<p>' + row.choiceD +'<p>');
		res.write('<p class=\"blank-page\">' + row.answer + '</p>');
		res.end('</html>');
	});
}).listen(1337);

console.log('Server running...');
