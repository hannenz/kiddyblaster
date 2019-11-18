/**
 * Default gulpfile for HALMA projects
 *
 * Version 2019-03-27
 *
 * @see https://www.sitepoint.com/introduction-gulp-js/
 * @see https://nystudio107.com/blog/a-gulp-workflow-for-frontend-development-automation
 * @see https://nystudio107.com/blog/a-better-package-json-for-the-frontend
 */
'use strict';

// package vars
const pkg = require('./package.json');
const fs = require('fs');

// gulp
const gulp = require('gulp');
const {series} = require('gulp');

// Load all plugins in 'devDependencies' into the variable $
const $ = require('gulp-load-plugins')({
	pattern: ['*'],
	scope: ['devDependencies'],
	rename: {
		'gulp-strip-debug': 'stripdebug'
	}
});

// Default error handler: Log to console
const onError = (err) => {
	console.log(err);
};

// A banner to output as header for dist files
const banner = [
	"/**",
	" * @project       <%= pkg.name %>",
	" * @author        <%= pkg.author %>",
	" * @build         " + $.moment().format("llll") + " ET",
	" * @release       " + $.gitRevSync.long() + " [" + $.gitRevSync.branch() + "]",
	" * @copyright     Copyright (c) " + $.moment().format("YYYY") + ", <%= pkg.copyright %>",
	" *",
	" */",
	""
].join("\n");


// var svgoOptions = {
// 	plugins: [
// 		{ cleanupIDs: false },
// 		{ mergePaths: false },
// 		{ removeViewBox: false },
// 		{ convertStyleToAttrs: false },
// 		{ removeUnknownsAndDefaults: false },
// 		{ cleanupAttrs: false }
// 	]
// };

// Project settings
var settings = {

	browserSync: {
		proxy: 'kiddyblaster-webui.localhost:4444',
		open: false,	// Don't open browser, change to "local" if you want or see https://browsersync.io/docs/options#option-open
		notify: false,	// Don't notify on every change
		https: {
			key: require('os').homedir() + '/server.key',
			cert: require('os').homedir() + '/server.crt'
		}
	},

	css: {
		src: './src/css/**/*.scss',
		dest: pkg.project_settings.prefix + 'css/',
		srcMain: [
			'./src/css/main.scss',
			// You can add more files here that will be built seperately,
			// f.e. newsletter.scss
		],
		options: {
			sass: {
				outputStyle: 'compact',
				precision: 3,
				errLogToConsole: true,
			},
			autoprefixer: pkg.browserslist
		},
		optionsProd: {
			sass: {
				outputStyle: 'compressed',
				precision: 3,
				errLogToConsole: true
			}
		}
	},

	js: {
		src:	'./src/js/*.js',
		dest:	pkg.project_settings.prefix + 'js/',
		destFile:	'main.min.js'
	},

	jsVendor: {
		src: [
			'./src/js/vendor/**/*.js',
			// Add single vendor files here,
			// they will be copied as is to `{prefix}/js/vendor/`,
			// e.g. './node_modules/flickity/dist/flickity.pkgd.min.js',
		],
		dest:	pkg.project_settings.prefix + 'js/vendor/'
	},

	cssVendor: {
		src:	[
			'./src/css/vendor/**/*.css',
			// Add single vendor files here,
			// they will be copied as is to `{prefix}/css/vendor/`,
			// e.g. './node_modules/flickity/dist/flickity.min.css'
		],
		dest:	pkg.project_settings.prefix + 'css/vendor/'
	},

	fonts: {
		src:	'./src/fonts/**/*',
		dest:	pkg.project_settings.prefix + 'fonts/'
	},

	// icons: {
	// 	src:	'./src/icons/**/*.svg',
	// 	dest:	pkg.project_settings.prefix + 'img/icons/',
	// 	options: [
	// 		$.imagemin.svgo(svgoOptions)
	// 	]
	// },
    //
  // image: {
  //   src: './src/img/',
  //   dest: pkg.project_settings.prefix + 'img/',
  //   sizes: [1920, 1024, 720, 320]
  // },
  //
	// sprite: {
	// 	src: './src/icons/*.svg',
	// 	dest: pkg.project_settings.prefix + 'img/',
	// 	destFile:	'icons.svg',
	// 	options: [
	// 		$.imagemin.svgo(svgoOptions)
	// 	]
	// },

	// favicons: {
	// 	src: './src/img/favicon.svg',
	// 	dest: pkg.project_settings.prefix + 'img/favicons/',
	// 	background: '#ffffff'
	// }
};



// Clean dist before building
function cleanDist() {
	return $.del([
		pkg.project_settings.prefix + '/'
	]);
}

/*
 *  Task: process SASS
 */
function cssDev() {
	return gulp
		.src(settings.css.srcMain)
		.pipe($.plumber({ errorHandler: onError}))
		.pipe($.sourcemaps.init())
		.pipe($.sass(settings.css.options.sass).on('error', $.sass.logError))
		.pipe($.autoprefixer(settings.css.options.autoprefixer))
		.pipe($.sourcemaps.write('./'))
		.pipe(gulp.dest(settings.css.dest))
		.pipe($.browserSync.stream());
}

function cssProd() {
	return gulp
		.src(settings.css.srcMain)
		.pipe($.plumber({ errorHandler: onError }))
		.pipe($.sass(settings.css.optionsProd.sass).on('error', $.sass.logError))
		.pipe($.autoprefixer(settings.css.options.autoprefixer))
		.pipe($.header(banner, { pkg: pkg }))
		.pipe(gulp.dest(settings.css.dest));
}

/*
 * Task: Concat and uglify Javascript
 */
function jsDev() {
	return gulp
		.src(settings.js.src)
		// .pipe($.jsvalidate().on('error', function(jsvalidate) { console.log(jsvalidate.message); this.emit('end'); }))
		.pipe($.sourcemaps.init())
		.pipe($.concat(settings.js.destFile))
		.pipe($.uglify().on('error', function(uglify) { console.log(uglify.message); this.emit('end'); }))
		.pipe($.sourcemaps.write('./'))
		.pipe(gulp.dest(settings.js.dest))
		.pipe($.browserSync.stream());
}

function jsProd() {
	return gulp
		.src(settings.js.src)
		.pipe($.jsvalidate().on('error', function(jsvalidate) { console.log(jsvalidate.message); this.emit('end'); }))
		.pipe($.concat(settings.js.destFile))
		.pipe($.stripdebug())
		.pipe($.uglify().on('error', function(uglify) { console.log(uglify.message); this.emit('end'); }))
		.pipe($.header(banner, { pkg: pkg }))
		.pipe(gulp.dest(settings.js.dest));
}

/*
 * Task: Uglify vendor Javascripts
 */
function jsVendor() {
	return gulp.src(settings.jsVendor.src)
		.pipe(gulp.dest(settings.jsVendor.dest));
}



function cssVendor() {
	return gulp.src(settings.cssVendor.src)
		.pipe(gulp.dest(settings.cssVendor.dest));
}



function fonts() {
	return gulp.src(settings.fonts.src)
		.pipe(gulp.dest(settings.fonts.dest));
}


// function imagesO(done) {
// 	gulp.src('src/img/**/*.svg')
// 		.pipe($.imagemin([
// 			$.imagemin.svgo(svgoOptions)
// 		]))
// 		.pipe(gulp.dest(pkg.project_settings.prefix + 'img/'));
//
// 	done();
// }

// function imagesOR(done) {
//
// 	[ 320, 720, 1024, 1920 ].forEach(function(size) {
// 		gulp.src('src/img/**/*.{png, gif, webp}')
// 			.pipe($.imageResize({ width: size, upscale: false }))
// 			.pipe($.rename(function(path) { path.basename = `${size}/${path.basename}`; }))
// 			.pipe($.imagemin([
// 				$.imagemin.optipng({ optimizationLevel: 1 }),
// 				$.imagemin.svgo(svgoOptions)
// 			]))
// 			.pipe(gulp.dest(pkg.project_settings.prefix + 'img/'));
// 	});
//
// 	done();
// }

// function imagesORC(done) {
//
// 	[ 320, 720, 1024, 1920 ].forEach(function(size) {
// 		gulp.src('src/img/**/*.{jpg, jpeg}')
// 			.pipe($.imageResize({ size: size, upscale: false }))
// 			.pipe($.rename(function(path) { path.basename = `${size}/${path.basename}`; }))
// 			.pipe($.imagemin([
// 				$.imagemin.optipng({ optimizationLevel: 1 }),
// 				$.imagemin.svgo(svgoOptions)
// 			]))
// 			.pipe(gulp.dest(pkg.project_settings.prefix + 'img/'))
// 			.pipe($.webp())
// 			.pipe(gulp.dest(pkg.project_settings.prefix + 'img/'));
// 	});
//
// 	done();
// }
//
//
// function icons() {
// 	return gulp.src(settings.icons.src)
// 		.pipe($.newer(settings.icons.dest))
// 		.pipe($.imagemin(settings.icons.options))
// 		.pipe(gulp.dest(settings.icons.dest));
// }


// /*
//  * Task: create sprites(SVG): optimize and concat SVG icons
//  */
// function sprite() {
// 	return gulp.src(settings.sprite.src)
// 		.pipe($.imagemin(settings.sprite.options))
// 		.pipe($.svgstore({
// 			inlineSvg: true
// 		}))
// 		.pipe($.rename(settings.sprite.destFile))
// 		.pipe(gulp.dest(settings.sprite.dest));
// }
//
//
//
/*
 * Default TASK: Watch SASS and JAVASCRIPT files for changes,
 * build CSS file and inject into browser
 */
function gulpDefault(done) {
  checkKey();
	$.browserSync.init(settings.browserSync);

	gulp.watch(settings.css.src, cssDev);
	gulp.watch(settings.js.src, jsDev);
  done();
}


/**
 * Generate favicons
 */
function favicons() {
	return gulp.src(settings.favicons.src)
		.pipe($.favicons({
			appName: pkg.name,
			appShortName: pkg.name,
			appDescription: pkg.description,
			developerName: pkg.author,
			developerUrl: pkg.repository.url,
			background: settings.favicons.background,
			path: settings.favicons.dest,
			url: pkg.project_settings.url,
			display: "standalone",
			orientation: "portrait",
			scope: "/",
			start_url: "/",
			version: pkg.version,
			logging: false,
			pipeHTML: false,
			replace: true,
			icons: {
				android: false,
				appleIcon: false,
				appleStartup: false,
				coast: false,
				firefox: false,
				windows: false,
				yandex: false,
				favicons: true
			}
		}))
		.pipe(gulp.dest(settings.favicons.dest));
}

// Check if SSL Key exists in default Directory
function checkKey()
{
  try {
  fs.accessSync(settings.browserSync.https.key, fs.constants.R_OK);
    console.log('https');
  } catch (err) {
    settings.browserSync.https = null;
    settings.browserSync.proxy = 'http://' + pkg.name + '.localhost';
    console.error('http');
  }
}


  function resolveAfter(x,y) {

    return new Promise(resolve => {
      setTimeout(() => {
        resolve(x);
      }, y*500);
    });

  }

  // Clean dist/img/ before building
  // function cleanImg(dest) {
  //   return new Promise(
  //     function(resolve) {
  //       resolve($.del(dest));
  //   });
  // }

  // OPTIMIZE
  // function opiImg() {
  //   return new Promise(function(resolve, reject) {
  //       gulp.src(settings.image.src + '*')
  //       .pipe($.newer(settings.image.dest))
  //       .pipe($.imagemin([$.imagemin.optipng({ optimizationLevel: 1 }),	$.imagemin.svgo(svgoOptions)]))
  //       .on('error', reject)
  //       .pipe(gulp.dest(settings.image.dest))
  //       .on('end', resolve);
  //   });
  // }
  //
  // // Resize All
  // function resizeImg(src, size) {
  //   return new Promise(function(resolve, reject) {
  //       gulp.src([src + '*.jpg', src + '*.png', src + '*.webp'])
  //       .pipe($.newer(settings.image.dest + size + '/'))
  //       .pipe($.imageResize({ width: size, upscale: false }))
  //       .on('error', reject)
  //       .pipe(gulp.dest(settings.image.dest + size + '/'))
  //       .on('end', resolve);
  //   });
  // }
  //
  // // Convert to webp
  // function webpImg() {
  //   return new Promise(function(resolve, reject) {
  //       gulp.src([settings.image.dest + '**/*.jpg', settings.image.dest + '**/*.png'])
  //       .pipe($.newer(settings.image.dest + '**/*.webp'))
  //       .pipe($.webp()).on('error', reject)
  //       .pipe(gulp.dest(settings.image.dest))
  //       .on('end', resolve);
  //   });
  // }
  //
  // const src = 'src/img/'; // set src
  // const dest = 'dist/img/'; // set dest
  // const sizes = [ 320, 720, 1024, 1920 ]; // set sizes

  // async function images() {
  //   try {
  //     //var a = cleanImg(dest);
  //     //await a;
  //     //console.log(await resolveAfter('Clean dest', 0));
  //
  //     var b = opiImg();
  //     await b;
  //     console.log(await resolveAfter('Optimzed imges', 0));
  //
  //
  //     var resizeSrc = settings.image.dest;
  //     for (var i = 0; i < settings.image.sizes.length; i++) {
  //       var l = resizeImg(resizeSrc, settings.image.sizes[i]);
  //       await l;
  //       console.log(await resolveAfter('Resized to ' + settings.image.sizes[i], 0));
  //       resizeSrc = settings.image.dest + settings.image.sizes[i] + '/';
  //     }
  //
  //     var d = webpImg();
  //     await d;
  //     console.log(await resolveAfter('Converted to webp', 0));
  //
  //   } catch (e) {
  //     console.log('error', e);
  //   }
  // }


//var exec = require('child_process').exec;

/*
 * Task: Build all
 */
exports.buildDev = series(cleanDist, jsDev, jsVendor, cssDev, cssVendor, fonts);
exports.buildProd = series(cleanDist, jsProd, jsVendor, cssProd, cssVendor, fonts);

exports.default = gulpDefault;
exports.cleanDist = cleanDist;
exports.cssDev = cssDev;
exports.cssProd = cssProd;
exports.jsDev = jsDev;
exports.jsProd = jsProd;
exports.jsVendor = jsVendor;
exports.cssVendor = cssVendor;
exports.fonts = fonts;

// exports.images = images;
//exports.images = series(cleanImg, imagesO, imagesOR, imagesORC);
// exports.optimizeImages = optimizeImages;
// exports.resizeImages = resizeImages;
// exports.convertImages = convertImages;
// exports.icons = icons;
// exports.sprite = sprite;
// exports.favicons = favicons;
