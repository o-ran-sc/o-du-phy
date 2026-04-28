from docs_conf.conf import *
linkcheck_ignore = [
    'http://localhost.*',
    'http://127.0.0.1.*',
    'https://gerrit.o-ran-sc.org.*',
    # Intel's web infrastructure (intel.com and the
    # registrationcenter*.intel.com / downloadcenter.intel.com
    # subdomains) actively blocks automated link probes via the
    # 'corpredirect.intel.com' 404 redirector and returns HTTP 403
    # for every request from Sphinx's linkcheck builder, regardless
    # of whether the underlying URL is valid for a real browser
    # session. Ignore the Intel host patterns to stop linkcheck
    # treating these references as broken links.
    r'https?://([a-z0-9-]+\.)*intel\.com/.*',
]
language = 'en'