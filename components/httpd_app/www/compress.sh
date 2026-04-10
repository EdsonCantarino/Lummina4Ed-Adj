#!/bin/bash

DIRECTORY=$(cd `dirname $0` && pwd)
echo $DIRECTORY

compress() {
    p1="$1"
    p2="$2"
        
    #if [ $p2 = "" ] then p2=$p1; fi

    cd $p1

    for file in $p2; do
        echo "Compressing: $file"
        cp "$file" "copy_$file" && \
        gzip -f "$file" && \
        mv "copy_$file" "$file"
    done

    cd $DIRECTORY
}

compress pages *.html
#compress pages *.js
#compress pages *.css
compress src/bootstrap/css *.css
#compress src/bootstrap/css/fonts *.eot
#compress src/bootstrap/css/fonts *.svg
#compress src/bootstrap/css/fonts *.ttf
compress src/bootstrap/css/fonts *.woff
compress src/bootstrap/css/fonts *.woff2
compress src/bootstrap/fonts *.woff
compress src/bootstrap/fonts *.woff2
compress src/bootstrap/js *.js
compress src/bootstrap_bundle *.js
compress src/bootstrap_datetimepicker *.js
compress src/bootstrap_datetimepicker *.css
compress src/jquery *.js
compress src/jqueryMultiLanguage *.js
compress src/moment *.js
compress src/chartjs *.js
compress src/custom *.js
compress src/locales *.json
compress resources *.png
