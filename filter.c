/*
 * This file is part of ltrace.
 * Copyright (C) 2012 Petr Machata, Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <stddef.h>
#include <error.h>
#include <assert.h>

#include "filter.h"
#include "library.h"

void abort(void); //xxx

void
filter_init(struct filter *filt)
{
	filt->rules = NULL;
}

void
filter_destroy(struct filter *filt)
{
	struct filter_rule *it;
	for (it = filt->rules; it != NULL; ) {
		struct filter_rule *next = it->next;
		filter_rule_destroy(it);
		it = next;
	}
}

void
filter_rule_init(struct filter_rule *rule, enum filter_rule_type type,
		 struct filter_lib_matcher *matcher,
		 regex_t symbol_re)
{
	rule->type = type;
	rule->lib_matcher = matcher;
	rule->symbol_re = symbol_re;
	rule->next = NULL;
}

void
filter_rule_destroy(struct filter_rule *rule)
{
	filter_lib_matcher_destroy(rule->lib_matcher);
	regfree(&rule->symbol_re);
}

void
filter_add_rule(struct filter *filt, struct filter_rule *rule)
{
	rule->next = filt->rules;
	filt->rules = rule;
}

void
filter_lib_matcher_name_init(struct filter_lib_matcher *matcher,
			     regex_t libname_re)
{
	matcher->type = FLM_NAME;
	matcher->libname_re = libname_re;
}

void
filter_lib_matcher_main_init(struct filter_lib_matcher *matcher)
{
	matcher->type = FLM_MAIN;
}

void
filter_lib_matcher_destroy(struct filter_lib_matcher *matcher)
{
	switch (matcher->type) {
	case FLM_NAME:
		regfree(&matcher->libname_re);
		break;
	case FLM_MAIN:
		break;
	}
}

static int
re_match_or_error(regex_t *re, const char *name, const char *what)
{
	int status = regexec(re, name, 0, NULL, 0);
	if (status == 0)
		return 1;
	if (status == REG_NOMATCH)
		return 0;

	char buf[200];
	regerror(status, re, buf, sizeof buf);
	error(0, 0, "Error when matching %s: %s", name, buf);

	return 0;
}

static int
matcher_matches_library(struct filter_lib_matcher *matcher, struct library *lib)
{
	switch (matcher->type) {
	case FLM_NAME:
		return re_match_or_error(&matcher->libname_re, lib->name,
					 "library name");
	case FLM_MAIN:
		assert(!"FLM_MAIN not implemented yet!");
		abort();
	}
	assert(matcher->type != matcher->type);
	abort();
}

int
filter_matches_library(struct filter *filt, struct library *lib)
{
	struct filter_rule *it;
	for (it = filt->rules; it != NULL; it = it->next)
		switch (it->type) {
		case FR_ADD:
			if (matcher_matches_library(it->lib_matcher, lib))
				return 1;
		case FR_SUBTRACT:
			continue;
		};
	return 0;
}

int
filter_matches_symbol(struct filter *filt, struct library_symbol *sym)
{
	int matches = 0;
	struct filter_rule *it;
	for (it = filt->rules; it != NULL; it = it->next) {
		switch (it->type) {
		case FR_ADD:
			if (matches)
				continue;
			break;
		case FR_SUBTRACT:
			if (!matches)
				continue;
		}

		if (matcher_matches_library(it->lib_matcher, sym->lib)
		    && re_match_or_error(&it->symbol_re, sym->name,
					 "symbol name"))
			matches = !matches;
	}
	return matches;
}