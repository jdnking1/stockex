---
layout: default
title: Home
---

# Stockex

Development notes, updates, and technical deep-dives on **Stockex**.

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
