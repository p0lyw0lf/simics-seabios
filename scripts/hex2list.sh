sed -e 's/}\;/]/' -e 's/{/[/' -e 's/static unsigned/ /' -e 's/short/ /' -e 's/char/ /' -e 's/^ *//' -e 's/\[\]/ /' < $1 > $2
