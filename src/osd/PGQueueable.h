// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2016 Red Hat Inc.
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */


#pragma once

#include <ostream>

#include "include/types.h"
#include "include/utime.h"
#include "osd/OpRequest.h"
#include "osd/PG.h"


class OSD;


struct PGScrub {
  epoch_t epoch_queued;
  explicit PGScrub(epoch_t e) : epoch_queued(e) {}
  ostream &operator<<(ostream &rhs) {
    return rhs << "PGScrub";
  }
};

struct PGSnapTrim {
  epoch_t epoch_queued;
  explicit PGSnapTrim(epoch_t e) : epoch_queued(e) {}
  ostream &operator<<(ostream &rhs) {
    return rhs << "PGSnapTrim";
  }
};

struct PGRecovery {
  epoch_t epoch_queued;
  uint64_t reserved_pushes;
  PGRecovery(epoch_t e, uint64_t reserved_pushes)
    : epoch_queued(e), reserved_pushes(reserved_pushes) {}
  ostream &operator<<(ostream &rhs) {
    return rhs << "PGRecovery(epoch=" << epoch_queued
	       << ", reserved_pushes: " << reserved_pushes << ")";
  }
};


class PGQueueable {
  typedef boost::variant<
    OpRequestRef,
    PGSnapTrim,
    PGScrub,
    PGRecovery
    > QVariant;
  QVariant qvariant;
  int cost;
  unsigned priority;
  utime_t start_time;
  entity_inst_t owner;
  unsigned dmclock_reservation, dmclock_weight, dmclock_limit;

  struct RunVis : public boost::static_visitor<> {
    OSD *osd;
    PGRef &pg;
    ThreadPool::TPHandle &handle;
    RunVis(OSD *osd, PGRef &pg, ThreadPool::TPHandle &handle)
      : osd(osd), pg(pg), handle(handle) {}
    void operator()(const OpRequestRef &op);
    void operator()(const PGSnapTrim &op);
    void operator()(const PGScrub &op);
    void operator()(const PGRecovery &op);
  }; // struct RunVis

  struct DisplayVis : public boost::static_visitor<> {
    std::ostream& out;
    DisplayVis(std::ostream& out) : out(out) {}
    void operator()(const OpRequestRef &op);
    void operator()(const PGSnapTrim &op);
    void operator()(const PGScrub &op);
    void operator()(const PGRecovery &op);
  }; // struct DisplayVis


public:

  explicit PGQueueable(OpRequestRef op)
    : qvariant(op), cost(op->get_req()->get_cost()),
      priority(op->get_req()->get_priority()),
      start_time(op->get_req()->get_recv_stamp()),
      owner(op->get_req()->get_source_inst()),
      dmclock_reservation(op->get_req()->get_dmclock_reservation()),
      dmclock_weight(op->get_req()->get_dmclock_weight()),
      dmclock_limit(op->get_req()->get_dmclock_limit())
  {}
  PGQueueable(
    const PGSnapTrim &op, int cost, unsigned priority, utime_t start_time,
    const entity_inst_t &owner)
    : qvariant(op), cost(cost), priority(priority), start_time(start_time),
      owner(owner) {}
  PGQueueable(
    const PGScrub &op, int cost, unsigned priority, utime_t start_time,
    const entity_inst_t &owner)
    : qvariant(op), cost(cost), priority(priority), start_time(start_time),
      owner(owner) {}
  PGQueueable(
    const PGRecovery &op, int cost, unsigned priority, utime_t start_time,
    const entity_inst_t &owner)
    : qvariant(op), cost(cost), priority(priority), start_time(start_time),
      owner(owner) {}

  friend std::ostream& operator<<(std::ostream&, const PGQueueable&);

  const boost::optional<OpRequestRef> maybe_get_op() const {
    const OpRequestRef *op = boost::get<OpRequestRef>(&qvariant);
    return op ? OpRequestRef(*op) : boost::optional<OpRequestRef>();
  }
  uint64_t get_reserved_pushes() const {
    const PGRecovery *op = boost::get<PGRecovery>(&qvariant);
    return op ? op->reserved_pushes : 0;
  }
  void run(OSD *osd, PGRef &pg, ThreadPool::TPHandle &handle) {
    RunVis v(osd, pg, handle);
    boost::apply_visitor(v, qvariant);
  }
  unsigned get_priority() const { return priority; }
  int get_cost() const { return cost; }
  utime_t get_start_time() const { return start_time; }
  entity_inst_t get_owner() const { return owner; }
  unsigned get_dmclock_reservation() const { return dmclock_reservation; }
  unsigned get_dmclock_weight() const { return dmclock_weight; }
  unsigned get_dmclock_limit() const { return dmclock_limit; }
  const QVariant& get_variant() const { return qvariant; }
}; // class PGQueueable


std::ostream& operator<<(std::ostream&, const PGQueueable&);
