## Apology

nwf is rather naive at all things JS, so any and/or all of this might be objectively bad code.
Please do feel free to replace any and/or all of it; they hope that what's here at least serve[ds] some illustrative purpose.

## What

The smartmeter tenants (grid, provider, user) all want to have more convenient interfaces than the CLI.
This is a (very skeletal, at the moment) start at providing those.

In particular, there is a NodeJS server that can turn POST requests of JavaScript code into Microvium bytecode and ship that over MQTT.

It is also configured to serve some static files,
of which there are precious few at the moment,
but which include in-browser MQTT support and a(n again very skeletal) grid MQTT subscriber.

Some configuration is shared between the in-browser clients and the server; see `static/democonfig.js`.

## Building

You'll want to ensure that you have NodeJS and NPM installed.
Running `npm install` will install our JS dependencies.

The `CodeMirror` editor we use to allow in-browser editing of JavaScript policy programs needs to be "bundled".
Towards that end, run

    ./node_modules/.bin/rollup --config ./static.in/user-editor.rollup-config.mjs

See https://codemirror.net/examples/bundle/ for details.

## Running

Then run

    node --experimental-require-module ./server.js

For NodeJS versions v23.0.0 or later, you can elide the `--experimental-require-module`.

You can test the Microvium compilation code, on the contents of `./foo.js`, with

    curl --verbose -H "Content-Type: text/plain" --data-binary @./foo.js http://localhost:4000/postjs/${METER_ID}

You can see the very basic grid client by visiting http://localhost:4000/static/grid.html

Similarly, the user view will be at http://localhost:4000/static/user-editor.html
