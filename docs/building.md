# Building

[Back to the README](../README.md)

With Docker:

```
docker build -t kaireader-devkit mupdf
docker tag kaireader-devkit kaireader-devkit:before-smb
docker build -t kaireader-devkit smb
docker build -t kaireader-host -f tests/Dockerfile.host tests
.\build.ps1            # KaiReader.nro
.\build.ps1 test       # host tests with sanitizers
.\build.ps1 preview    # the screens, drawn on the PC
```

The `kaireader-devkit` image starts from KaiRadio's image (the Switch 2
devkit with libounce). The tests open made-up books written by
`tests/make_fixtures.py`.

On every push and pull request, GitHub Actions runs the host tests
(`make coverage`) in an image built from `tests/Dockerfile.host`, with
MuPDF kept in its cache (`.github/workflows/tests.yml`).
