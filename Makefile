WORKSPACE ?= workspace.dsl
STRUCTURIZR ?= ./tools/structurizr-cli/structurizr.sh

.PHONY: validate-arch inspect-arch check-arch

validate-arch:
	@test -x "$(STRUCTURIZR)" || (echo "Structurizr CLI not found at $(STRUCTURIZR). Install it first."; exit 1)
	@$(STRUCTURIZR) validate -workspace $(WORKSPACE)

inspect-arch:
	@test -x "$(STRUCTURIZR)" || (echo "Structurizr CLI not found at $(STRUCTURIZR). Install it first."; exit 1)
	@$(STRUCTURIZR) inspect -workspace $(WORKSPACE)

check-arch: validate-arch
