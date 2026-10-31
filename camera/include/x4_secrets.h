// Picks secrets.h if present (gitignored), else the committed example so CI and fresh clones build.
#pragma once
#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "camera/include/secrets.h not found: using secrets.h.example (default AP password!)"
#include "secrets.h.example"
#define X4_SECRETS_ARE_DEFAULT 1
#endif
#ifndef X4_SECRETS_ARE_DEFAULT
#define X4_SECRETS_ARE_DEFAULT 0
#endif
