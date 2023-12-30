## Introduction

This is a proof-of-concept showing how to use `nsjail` to isolate builds.

## Security concerns

Using `nsjail` as the isolation layer is predicated on the fact that we trust
`pnpm install`, which means that it won't allow some way to escape out of the
shell.

It's also predicated on the fact that we trust our own build process, in
particular our use of `esbuild`.

## Steps

1. `docker build -t nsjailcontainer . `
2. `docker run -v ./customer-project:/customer-project -v ./zuplo-config:/zuplo-config --rm -it nsjailcontainer nsjail --config /zuplo-config/build.cfg`
