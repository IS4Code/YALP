YALP v0.1
==========

_YALP_, short for _Yet Another Lua Plugin_, aims to be a simple yet extendable SA-MP plugin allowing to use Lua for SA-MP server programming.

Compared to older Lua plugins, _YALP_ doesn't export any of the SA-MP natives or callbacks directly. Instead, it exposes a set of functions to interact with the server via a virtual filterscript, mimicking calls to AMX functions. The actual server API can be ported entirely with Lua, which removes the need to update the plugin for new versions of SA-MP and maintain all functions, and also allows the use of other plugins.

## Installation
Download the latest [release](//github.com/IllidanS4/YALP/releases/latest) for your platform to the "plugins" directory and add "YALP" (or "YALP.so" on Linux) to the `plugins` line in server.cfg.

Include [YALP.inc](pawno/include/YALP.inc) to create and control Lua machines on-the-fly.

## Building
Use Visual Studio to build the project on Windows, or `make` on Linux.
