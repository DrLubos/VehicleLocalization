import React, { useState, useEffect } from "react";
import {
  Modal,
  ModalOverlay,
  ModalContent,
  ModalHeader,
  ModalCloseButton,
  ModalBody,
  ModalFooter,
  FormControl,
  FormLabel,
  Input,
  Button,
  Select,
  Checkbox,
  VStack,
  HStack
} from "@chakra-ui/react";

const predefinedColors = [
  "#FFFFFF",
  "#FF0000",
  "#00FF00",
  "#0000FF",
  "#FFFF00",
  "#FF00FF",
  "#00FFFF",
  "#000000",
  "#808080",
  "#800000",
  "#008000",
  "#000080"
];

const VehicleFormModal = ({ isOpen, onClose, onSubmit, initialData = {}, isEdit = false }) => {
  const defaultPosFreq = "15";
  const defaultMinDelta = "3";
  const defaultMaxIdle = "15";

  const [name, setName] = useState(initialData.name || "");
  const [imei, setImei] = useState(initialData.imei || "");
  const [color, setColor] = useState(initialData.color || predefinedColors[0]);
  const [positionCheckFreq, setPositionCheckFreq] = useState(
    initialData.position_check_freq !== undefined ? String(initialData.position_check_freq) : defaultPosFreq
  );
  const [minDistanceDelta, setMinDistanceDelta] = useState(
    initialData.min_distance_delta !== undefined ? String(initialData.min_distance_delta) : defaultMinDelta
  );
  const [maxIdleMinutes, setMaxIdleMinutes] = useState(
    initialData.max_idle_minutes !== undefined ? String(initialData.max_idle_minutes) : defaultMaxIdle
  );
  const [manualRouteStartEnabled, setManualRouteStartEnabled] = useState(initialData.manual_route_start_enabled || false);

  const [hasInitialized, setHasInitialized] = useState(false);
  useEffect(() => {
    if (isOpen && !hasInitialized) {
      setName(initialData.name || "");
      setImei(initialData.imei || "");
      setColor(initialData.color || predefinedColors[0]);
      setPositionCheckFreq(
        initialData.position_check_freq !== undefined ? String(initialData.position_check_freq) : defaultPosFreq
      );
      setMinDistanceDelta(
        initialData.min_distance_delta !== undefined ? String(initialData.min_distance_delta) : defaultMinDelta
      );
      setMaxIdleMinutes(
        initialData.max_idle_minutes !== undefined ? String(initialData.max_idle_minutes) : defaultMaxIdle
      );
      setManualRouteStartEnabled(initialData.manual_route_start_enabled || false);
      setHasInitialized(true);
    }
  }, [isOpen, initialData, hasInitialized]);

  useEffect(() => {
    if (!isOpen) {
      setHasInitialized(false);
    }
  }, [isOpen]);

  const handleSubmit = (e) => {
    e.preventDefault();
    const posFreq = parseInt(positionCheckFreq, 10);
    const minDelta = parseInt(minDistanceDelta, 10);
    const idleMin = parseInt(maxIdleMinutes, 10);
    if (isNaN(posFreq) || isNaN(minDelta) || isNaN(idleMin)) {
      alert("Please enter valid integer values for frequency, distance delta, and idle minutes.");
      return;
    }
    const formData = {
      name,
      imei,
      color,
      position_check_freq: posFreq,
      min_distance_delta: minDelta,
      max_idle_minutes: idleMin,
      manual_route_start_enabled: manualRouteStartEnabled
    };
    onSubmit(formData);
  };

  return (
    <Modal isOpen={isOpen} onClose={onClose} isCentered size="lg">
      <ModalOverlay />
      <ModalContent>
        <ModalHeader>{isEdit ? "Edit Vehicle" : "Add Vehicle"}</ModalHeader>
        <ModalCloseButton />
        <form onSubmit={handleSubmit}>
          <ModalBody>
            <VStack spacing={4}>
              <FormControl id="name" isRequired>
                <FormLabel>Name</FormLabel>
                <Input value={name} onChange={(e) => setName(e.target.value)} />
              </FormControl>
              <FormControl id="imei" isRequired>
                <FormLabel>IMEI</FormLabel>
                <Input
                  value={imei}
                  onChange={(e) => setImei(e.target.value)}
                  isReadOnly={isEdit}
                />
              </FormControl>
              <FormControl id="color" isRequired>
                <FormLabel>Color</FormLabel>
                <Select value={color} onChange={(e) => setColor(e.target.value)}>
                  {predefinedColors.map((c) => (
                    <option key={c} value={c}>
                      {c}
                    </option>
                  ))}
                </Select>
              </FormControl>
              <HStack spacing={4} width="100%">
                <FormControl id="positionCheckFreq" isRequired>
                  <FormLabel>Position Check Frequency (s)</FormLabel>
                  <Input
                    type="number"
                    pattern="[0-9]*"
                    inputMode="numeric"
                    value={positionCheckFreq}
                    onChange={(e) => setPositionCheckFreq(e.target.value)}
                    placeholder="(1 - 255)"
                  />
                </FormControl>
                <FormControl id="minDistanceDelta" isRequired>
                  <FormLabel>Min Distance Delta (m)</FormLabel>
                  <Input
                    type="number"
                    pattern="[0-9]*"
                    inputMode="numeric"
                    value={minDistanceDelta}
                    onChange={(e) => setMinDistanceDelta(e.target.value)}
                    placeholder="(0 - 255)"
                  />
                </FormControl>
                <FormControl id="maxIdleMinutes" isRequired>
                  <FormLabel>Max Idle Minutes</FormLabel>
                  <Input
                    type="number"
                    pattern="[0-9]*"
                    inputMode="numeric"
                    value={maxIdleMinutes}
                    onChange={(e) => setMaxIdleMinutes(e.target.value)}
                    placeholder="(1 - 255)"
                  />
                </FormControl>
              </HStack>
              <FormControl id="manualRouteStartEnabled">
                <FormLabel>Manual Route Start Enabled</FormLabel>
                <Checkbox
                  isChecked={manualRouteStartEnabled}
                  onChange={(e) => setManualRouteStartEnabled(e.target.checked)}
                />
              </FormControl>
            </VStack>
          </ModalBody>
          <ModalFooter>
            <Button colorScheme="blue" mr={3} type="submit">
              {isEdit ? "Save Changes" : "Add Vehicle"}
            </Button>
            <Button variant="ghost" onClick={onClose}>
              Cancel
            </Button>
          </ModalFooter>
        </form>
      </ModalContent>
    </Modal>
  );
};

export default VehicleFormModal;
