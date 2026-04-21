## Create Debian Package Structure
```
dh_make -i --createorig -e info@myridia.com -c gpl3 -y 
```
## Build the Package
```
dpkg-buildpackage -us -uc -b

```

## After install the deb package into the os
```
sudo dpkg -i ftl-maker_1.2-1_all.deb 
```

## Review the installed files of the installed deb package
```
dpkg -L ftl-maker
```

## Remove the deb package from the os
```
sudo apt-get remove ftl-maker  --purge
```
