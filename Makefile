SHELL	= /bin/sh

PINED_SRC=./pined
LIBPS_SRC=./libpinesock

all	:
	cd $(PINED_SRC) && $(MAKE)
	cd $(LIBPS_SRC) && $(MAKE)
	cd test && $(MAKE)
	cd samples && $(MAKE)

clean :
	cd $(PINED_SRC) && $(MAKE) clean
	cd $(LIBPS_SRC) && $(MAKE) clean
	cd test && $(MAKE) clean
	cd samples && $(MAKE) clean
	
clean-all :
	cd $(PINED_SRC) && $(MAKE) clean-all
	cd $(LIBPS_SRC) && $(MAKE) clean-all
	cd test && $(MAKE) clean-all
	cd samples && $(MAKE) clean-all
	rm *~

PINED_VER=`cat VERSION`
PINED_DIR=pined-$(PINED_VER)
package	: clean-all
	@echo "creating package..."
	@echo "const char version_string[] = \"$(PINED_VER)\";" > $(PINED_SRC)/version.c
	tar zcvf pined.tar.gz ./ \
		--exclude .svn		\
		--exclude *~		\
		--exclude htdoc		\
		--exclude pined.tar.gz	\
		--exclude pined*.tar.gz	\
		--exclude $(PINED_DIR)
	@if [ -d $(PINED_DIR) ]; then rm -rf $(PINED_DIR); fi
	@mkdir $(PINED_DIR)
	@cd $(PINED_DIR) && tar zxvf ../pined.tar.gz
	tar zcvf $(PINED_DIR).tar.gz $(PINED_DIR)
	@rm -rf $(PINED_DIR) pined.tar.gz
	@echo "done."
