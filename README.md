# PintOS

### Run docker container
```
docker run --rm --mount type=bind,source=C:\Users\USUARIO\OneDrive\Escritorio\pintos\src,target=/pintos -w /pintos -i -t gbenm/pintos bash 
```

## Create and format a filesystem
```
pintos-mkdisk filesys.dsk --filesys-size=2
pintos -- -f -q
```

## Copy files into filesystem
Let's copy echo program from examples/ . Rember to do make into examples/ directory at least onces.
```
pintos -p ../../examples/echo -a echo -- -q
```

## Running a program
```
pintos -- -q run 'echo PKUOS'
```
