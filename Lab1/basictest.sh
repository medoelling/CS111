true # blah blah

g++ -c foo.c #blah blah blah

: : :



if cat < /etc/passwd | tr a-z A-Z | sort -u; then :; else echo sort failed!; fi

a b<c > d

if cat < /etc/passwd | tr a-z A-Z | sort -u > out
then : # blah
else echo sort failed!
fi


if
  if a;a;a; then b; else :; fi
then
  if c                      # blah blah
  then if d | e; then f; fi
 fi
fi


g<h

while
  while
    until :; do echo yoo hoo!; done
    false
  do (a|b)
  done >f
do
  :>g
done

# Another weird example: nobody would ever want to run this.
a<b>c|d<e>f|g<h>i

