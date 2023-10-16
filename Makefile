
build:
	@echo Build Docker
	docker build -t nethack-custom -f docker/Dockerfile .
	@mkdir -p save

sh:
	@docker run -it --rm \
		-v "`pwd`":"/host_shared" \
		nethack-custom bash

run:
	@docker run -it --rm \
		-v "`pwd`/save":"/root/nh/install/games/lib/jnethackdir/save" \
		-v "`pwd`":"/host_shared" \
		 nethack-custom /root/nh/install/games/jnethack
