all:
	git pull
	hugo
	cd tools&&python3 ac.py&&cd ..

test:
	git pull
	hugo serve