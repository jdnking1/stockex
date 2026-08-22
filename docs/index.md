--
layout: default
title: Home
---

# StockeX

**A low-latency C++23 matching engine, built for nanosecond-level determinism.**

StockeX is a stock exchange matching engine simulator, designed as a research platform for high-performance trading system architecture. It's built to process millions of order events per second with predictable, sub-microsecond latency — the same class of problem real HFT infrastructure has to solve.

This blog covers the architectural decisions behind it: why certain data structures were chosen, what trade-offs came up, and what the benchmarks actually show.

## Latest Posts

<ul>
{% for post in site.posts %}
  <li>
    <a href="{{ post.url | relative_url }}">{{ post.title }}</a>
    <br>
    <small>{{ post.date | date: "%B %-d, %Y" }}</small>
  </li>
{% endfor %}
</ul>
