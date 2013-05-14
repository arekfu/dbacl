#!/bin/bash
# quick script to plot the scores of training documents.
# needs mbox format file on command line.

DBACL=/home/laird/proj/cpp/dbacl/src/dbacl
DATAFILE=/home/laird/dummy.scores
PVALUES=/home/laird/dummy.pvalues
RESFILE=/home/laird/dummy.results
SMPMV=/home/laird/dummy.samplemv
CMDFILE=/home/laird/dummy.gnuplot

if [ ! -e $DBACL ] ; then 
    echo "$DBACL not found!"
    exit 1
fi
if [ -z "$1" ] ; then
    echo "usage: $0 MBOX-FILE"
    exit 1
fi

# uncomment this for the MULTINOMIAL model
#cat $@ | $DBACL -T email -T email:headers -T html:links -H 25 -e adp -l dummy -0 -v -X -M
#cat $@ | formail -s $DBACL -c dummy -m -n -v | awk '{print $2}' | freq > $DATAFILE

# uncomment this for the SEQUENTIAL model
cat $@ | $DBACL -T email -T email:headers -T html:links -H 25 -e adp -l dummy -0 -v -X | grep reservoir > dummy.reservoir
cat $@ | formail -s $DBACL -c dummy -m -X -v > $RESFILE
cat $RESFILE | awk '{print $3}' | freq > $DATAFILE
cat $RESFILE | tr -s ']%[' ' ' | awk '{print $3, $8}' > $PVALUES
cat $RESFILE | awk '{n++; d += $3; dd += $3*$3;}END{print d/n, ((n*dd -(d*d))/(n*(n-1)))}' > $SMPMV

MALPHA=`head -5 $DBACL_PATH/dummy | grep shannon | awk '{print $5}'`
MBETA=`head -5 $DBACL_PATH/dummy | grep shannon | awk '{print $7}'`
MMU=`head -5 $DBACL_PATH/dummy | grep shannon | awk '{print $9}'`
MS2=`head -5 $DBACL_PATH/dummy | grep shannon | awk '{print $11}'`

echo "ALPHA $MALPHA BETA $MBETA MU $MMU S2 $MS2"

MU=`cat $SMPMV | awk '{print $1}'`
SIGMA2=`cat $SMPMV | awk '{print $2}'`

cat > $CMDFILE <<EOF
set title "$@ alpha=$ALPHA, beta=$BETA, mu=$MU, sigma2=$SIGMA2"
set terminal x11

a = $MMU/log(2)
b = $MS2/log(2)
ga = $MALPHA/log(2)
gb = $MBETA

c = 20
d = 20
f = 20

ra = $MU
rb = $SIGMA2
rgb = rb/ra
rga = (ra**2)/rb

rc = 20
rd = 20
rf = 20

mynorm(x) = c*exp(-0.5*(((x-a)/b)**2))
mygam(x) = d*exp(-(x/gb)-lgamma(ga)+(ga-1)*log(x/gb))
mychi(x) = f*exp(-(x/2)-lgamma(a/2)+((a/2)-1)*log(x/2))

rnorm(x) = rc*exp(-0.5*(((x-ra)/rb)**2))
rgam(x) = rd*exp(-(x/rgb)-lgamma(rga)+(rga-1)*log(x/rgb))
rchi(x) = rf*exp(-(x/2)-lgamma(ra/2)+((ra/2)-1)*log(x/2))

fit mynorm(x) "$DATAFILE" using 1:2 via c
fit mygam(x) "$DATAFILE" using 1:2 via d
#fit mychi(x) "$DATAFILE" using 1:2 via f

fit rnorm(x) "$DATAFILE" using 1:2 via rc
fit rgam(x) "$DATAFILE" using 1:2 via rd
#fit rchi(x) "$DATAFILE" using 1:2 via rf

plot "$DATAFILE" with impulses, "$PVALUES" using (\$1):(\$2*c/100) with impulses, mynorm(x), mygam(x), rnorm(x), rgam(x)

pause -1 "press any key"
EOF

gnuplot $CMDFILE