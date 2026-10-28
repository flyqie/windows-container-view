#pragma once

// Registers the RPC interface and starts listening. Returns once the server is
// listening; throws the RPC status if it cannot.
void SandboxInitServer();

// Creates the window and runs the render loop. Does not return.
void SandboxInitVideo();
