<?xml version="1.0" encoding="UTF-8"?>
<!--
  Originated from https://github.com/rpavlik/jenkins-ctest-plugin
  MIT License added to top of this file from repo's LICENSE file
-->

<!--
Copyright (c) 2010 VersionOne, Inc.
Copyright (c) 2011, 2014 Ryan Pavlik <abiryan@ryand.net.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-->
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
    <xsl:output method="xml" indent="yes" />
    <xsl:template match="/">
        <testsuites>
            <xsl:variable name="buildName" select="//Site/@BuildName"/>
            <xsl:variable name="numberOfTests" select="count(//Site/Testing/Test)"/>
            <xsl:variable name="numberOfFailures" select="count(//Site/Testing/Test[@Status!='passed'])" />
            <testsuite name="CTest"
                tests="{$numberOfTests}" time="0"
                failures="{$numberOfFailures}"  errors="0"
                skipped="0">
            <xsl:for-each select="//Site/Testing/Test">
                    <xsl:variable name="testName" select="translate(Name, '-', '_')"/>
                    <xsl:variable name="duration" select="Results/NamedMeasurement[@name='Execution Time']/Value"/>
                    <xsl:variable name="status" select="@Status"/>
                    <xsl:variable name="output" select="Results/Measurement/Value"/>
                    <xsl:variable name="className" select="translate(Path, '/.', '.')"/>
                    <testcase classname="projectroot{$className}"
                        name="{$testName}"
                        time="{$duration}">
                        <xsl:if test="@Status!='passed'">
                            <failure>
                                <xsl:value-of select="$output" />
                            </failure>
                        </xsl:if>
                        <system-out>
                            <xsl:value-of select="$output" />
                        </system-out>
                    </testcase>
                </xsl:for-each>
            </testsuite>
        </testsuites>
    </xsl:template>
</xsl:stylesheet>
