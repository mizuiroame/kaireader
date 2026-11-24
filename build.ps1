# Builds KaiReader.nro with the kaireader-devkit Docker image (the KaiRadio
# image plus MuPDF, from mupdf/Dockerfile).
#   .\build.ps1           build the .nro
#   .\build.ps1 test      run the host tests (kaireader-host image, tests/Dockerfile.host)
#   .\build.ps1 preview   render the screens to tests\preview\build\shots\*.png
#   .\build.ps1 clean     remove build output
param([ValidateSet('nro', 'test', 'preview', 'clean')][string]$Target = 'nro')

$ErrorActionPreference = 'Stop'
$project = $PSScriptRoot
$devkit = 'kaireader-devkit'
$host_image = 'kaireader-host'

foreach ($image in @($devkit, $host_image)) {
    docker image inspect $image *> $null
    if ($LASTEXITCODE -ne 0) {
        throw "Docker image '$image' not found. Build it with: docker build -t $devkit mupdf; docker build -t $host_image -f tests/Dockerfile.host tests"
    }
}

$mount = "${project}:/build/kaireader"
switch ($Target) {
    'nro'     { docker run --rm -v $mount -w /build/kaireader $devkit make -j8 }
    'test'    { docker run --rm --security-opt seccomp=unconfined -v $mount -w /build/kaireader/tests $host_image make coverage }
    'preview' { docker run --rm -v $mount -w /build/kaireader/tests/preview $host_image make }
    'clean'   { docker run --rm -v $mount -w /build/kaireader $devkit sh -c 'make clean && rm -rf tests/build tests/preview/build' }
}
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit code $LASTEXITCODE)." }
