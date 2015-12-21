#!/bin/bash

FB=fb2
NETWORK_CONTENT=~/tmp/bing5.ts
AD=~/tmp/ad.ts
DST=~/tmp/splice
SPLICER=/usr/local/google/home/chema/proj/m2pb/tools/splice.py
INDEXER=~/proj/gfiber-core/vendor/google/mcastcapture/parser/index_file

declare -A SHASH
SHASH=(
		[splice_in.short.ts]="-i $AD:-1:-1 -i $NETWORK_CONTENT:1629000:-1 --splice-frames 3.5"
		[splice_in.long.ts]="-i $AD:-1:-1 -i $NETWORK_CONTENT:1662000:-1 --splice-frames 13"
)
	#	[splice_in.short.ts]="-i $AD:-1:-1 -i $NETWORK_CONTENT:1629000:-1 --splice-frames 3.5"
	#	[splice_in.long.ts]="-i $AD:-1:-1 -i $NETWORK_CONTENT:1662000:-1 --splice-frames 13"
	#	[splice_out.short.ts]="-i $NETWORK_CONTENT:-1:1629000 -i $AD:-1:-1"
	#	[splice_out.long.ts]="-i $NETWORK_CONTENT:-1:1662000 -i $AD:-1:-1"


function get_duration {
	local filename=$1
	str=$(ffmpeg -i $filename |& sed -n -e 's/Duration: \([^,]*\),.*/\1/p' | sed -n -e 's/ *00:00:\([^ ]*\)/\1/p')
	echo "$str"
	return
}

for f in "${!SHASH[@]}"; do
	#echo "  - $f gets mapped to ${SHASH[$f]}"; done
	echo $SPLICER ${SHASH[$f]} -o $DST/$f
	$SPLICER ${SHASH[$f]} -o $DST/$f
	$INDEXER $DST/$f > /dev/null
	duration=$(get_duration $DST/$f)
	echo ssh $FB /app/sage/mints_client -d --media-insert --media-item /var/media/ads/$f:0:$duration
	rsync -avut $DST/ $FB:/var/media/ads/ > /dev/null
	ssh $FB chown video:video /var/media/ads/splice\*
	ssh $FB chmod 644 /var/media/ads/splice\*
done

