<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:ss="http://docbook.sf.net/xmlns/string.subst/1.0" version="1.0">
  <xsl:import href="http://docbook.sourceforge.net/release/xsl-ns/current/manpages/profile-docbook.xsl"/>
  <xsl:param name="version"/>
  <xsl:param name="man.string.subst.map.local.pre">
    <ss:substitution oldstring="%version%" newstring="{$version}" />
  </xsl:param>
  <!-- man.th.title.max.length defaults to 20 and truncates the .TH title
       after groff-escaping, which cuts "metrics-exporter.conf" down to
       "metrics-exporter." and drops the ".conf" suffix. Disable the
       truncation so long page names are not mangled. -->
  <xsl:param name="man.th.title.max.length"></xsl:param>
</xsl:stylesheet>
