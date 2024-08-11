if [ -n "$WIZ_PATH" ]
then
	DATA_PATH="$WIZ_PATH"
elif [ -n "$XDG_DATA_HOME" ]
then
	DATA_PATH="$XDG_DATA_HOME"/wiz.csv
else
	DATA_PATH="$HOME"/.local/share/wiz.csv
fi
rm ${DATA_PATH}
sudo rm /usr/local/bin/wiz
