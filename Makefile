setup:
	docker compose up -d

shell:
	docker compose exec node1 bash

clean:
	docker compose down -v --rmi all --remove-orphans
