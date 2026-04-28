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

# Allow expected permanent redirects so 'sphinx-build -W -b linkcheck'
# does not flag them as warnings. Sources are migrated to the canonical
# destinations where editorially clean; redirects that cannot be cleanly
# updated in fixed-width RST tables (e.g. 3GPP) are tolerated here.
linkcheck_allowed_redirects = {
    # O-RAN-SC Confluence Cloud migration (legacy host -> Atlassian Cloud)
    r'https://wiki\.o-ran-sc\.org/.*': r'https://lf-o-ran-sc\.atlassian\.net/.*',
    # O-RAN-SC JIRA Cloud migration (legacy host -> Atlassian Cloud)
    r'https://jira\.o-ran-sc\.org/.*': r'https://lf-o-ran-sc\.atlassian\.net/.*',
    # 3GPP reorganised the 'specifications' landing page; the canonical
    # URL is '/specifications-technologies' but the cell width in the
    # overview1.rst RST table cannot fit the longer URL without
    # restructuring the whole table.
    r'https://www\.3gpp\.org/specifications': r'https://www\.3gpp\.org/specifications-technologies',
}
language = 'en'
